// 任务系统调试工具（仅非 Shipping 编译）
//
// 本文件是所有 Task.* 调试控制台命令的注册入口与实现：
//   Task.Help / Task.Dump / Task.Validate [AssetPath] / Task.Skip <N> /
//   Task.Finish <ClassName|All> / Task.Restart / Task.Roles / Task.Panels /
//   Task.Scores / Task.Debug <0|1>（cvar，场景 Tag/区域可视化 + 左上角 HUD）。
// 校验逻辑按任务类逐个 Cast 检查配表问题（缺引用、空配置、非法数值等），
// 结果以 [错误]/[警告]/[提示] 前缀输出。模块启动时调用
// FTaskDebug::RegisterConsoleCommands() 注册（幂等）。Shipping 构建下
// 全部内容被 #if !UE_BUILD_SHIPPING 裁剪，不影响正式包。

#include "Debug/TaskDebug.h"

#if !UE_BUILD_SHIPPING

#include "MultipleTask/TaskSequencerComponent.h"
#include "MultipleTask/Data/PrimaryTaskDataAsset.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask/TaskItems/MultipleTask_UIPanelDisplay.h"
#include "MultipleTask/TaskItems/MultipleTask_WidgetDisplay.h"
#include "MultipleTask/TaskItems/MultipleTask_WaitForConfirm.h"
#include "MultipleTask/TaskItems/MultipleTask_RoleSelect.h"
#include "MultipleTask/TaskItems/MultipleTask_MultiStationArrival.h"
#include "MultipleTask/TaskItems/MultipleTask_StationConfirm.h"
#include "MultipleTask/TaskItems/MultipleTask_GuideToLocation.h"
#include "MultipleTask/TaskItems/MultipleTask_EquipmentInteract.h"
#include "MultipleTask/TaskItems/MultipleTask_EquipmentConfirm.h"
#include "MultipleTask/TaskItems/MultipleTask_ZoneMonitor.h"
#include "MultipleTask/TaskItems/MultipleTask_SearchInspection.h"
#include "MultipleTask/TaskItems/MultipleTask_NPCBehavior.h"
#include "MultipleTask/TaskItems/MultipleTask_PlayAnim.h"
#include "MultipleTask/TaskItems/MultipleTask_PlayAudio.h"
#include "MultipleTask/TaskItems/MultipleTask_VoiceRecognitionWait.h"
#include "MultipleTask/TaskItems/MultipleTask_EndTraining.h"
#include "MultipleTask/Score/ScoreComponent.h"
#include "MultipleTask/UI/TaskPanelChannelComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

// ==================== cvar ====================

static TAutoConsoleVariable<int32> CVarTaskDebug(
	TEXT("Task.Debug"),
	0,
	TEXT("任务系统调试可视化：0=关闭，1=显示 Tag/区域/HUD"),
	ECVF_Cheat);

// ==================== 内部工具 ====================

namespace
{
	void AddIssue(TArray<FTaskDebugIssue>& Issues, ETaskDebugSeverity Severity,
		const FString& Location, const FString& Message)
	{
		FTaskDebugIssue& Issue = Issues.AddDefaulted_GetRef();
		Issue.Severity = Severity;
		Issue.Location = Location;
		Issue.Message = Message;
	}

	FString MakeStepLocation(int32 StepIndex, const FTaskStep& Step)
	{
		return FString::Printf(TEXT("Step[%02d] %s"), StepIndex, *Step.StepId.ToString());
	}

	const TCHAR* SeverityPrefix(ETaskDebugSeverity Severity)
	{
		switch (Severity)
		{
		case ETaskDebugSeverity::Error:   return TEXT("[错误]");
		case ETaskDebugSeverity::Warning: return TEXT("[警告]");
		default:                          return TEXT("[提示]");
		}
	}

	/** 场景中是否存在带该 Tag 的 Actor */
	bool ActorWithTagExists(UWorld* World, FName Tag)
	{
		if (!World || Tag.IsNone())
		{
			return false;
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(Tag))
			{
				return true;
			}
		}
		return false;
	}

	bool HasScoreRule(const UPrimaryTaskDataAsset* Asset, FName ErrorCode)
	{
		if (!Asset || ErrorCode.IsNone())
		{
			return false;
		}

		for (const FTaskStep& Step : Asset->Steps)
		{
			for (const FScoreRule& Rule : Step.ScoreRules)
			{
				if (Rule.ErrorCode == ErrorCode)
				{
					return true;
				}
			}
		}
		return false;
	}

	/** 身份 → 站位 配置校验（多人到位 / 就位确认共用） */
	void ValidateStationRequirements(UWorld* World, const TArray<FRoleStationPair>& Requirements,
		const FString& Loc, TArray<FTaskDebugIssue>& Issues)
	{
		if (Requirements.Num() == 0)
		{
			AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("Requirements 为空，任务会一直等待"));
			return;
		}

		for (int32 Index = 0; Index < Requirements.Num(); ++Index)
		{
			const FRoleStationPair& Pair = Requirements[Index];

			if (Pair.RoleTag.IsNone())
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc,
					FString::Printf(TEXT("Requirements[%d].RoleTag 为空"), Index));
			}

			if (Pair.StationTag.IsNone())
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc,
					FString::Printf(TEXT("Requirements[%d].StationTag 为空"), Index));
			}
			else if (World && !ActorWithTagExists(World, Pair.StationTag))
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
					FString::Printf(TEXT("场景中找不到带 Tag %s 的站位碰撞盒"), *Pair.StationTag.ToString()));
			}
		}
	}

	/** 单个任务的字段校验 */
	void ValidateTask(UWorld* World, const UPrimaryTaskDataAsset* Asset, int32 StepIndex,
		const FTaskStep* Step, UMultipleTaskBase* Task, TArray<FTaskDebugIssue>& Issues)
	{
		if (!Task)
		{
			return;
		}

		const FString Loc = Step
			? FString::Printf(TEXT("Step[%02d] %s / %s"), StepIndex, *Step->StepId.ToString(), *Task->GetClass()->GetName())
			: FString::Printf(TEXT("Step[%02d] 运行中 / %s"), StepIndex, *Task->GetClass()->GetName());

		// ---------- 面板展示 ----------
		if (const UMultipleTask_UIPanelDisplay* Panel = Cast<UMultipleTask_UIPanelDisplay>(Task))
		{
			// 非阻塞任务：AutoCloseDelay=0（跟随步骤生命周期）与 >0（定时关闭）均为合法配置
			if (Panel->AutoCloseDelay > 0.f && Panel->AutoCloseDelay < 0.5f)
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
					TEXT("AutoCloseDelay 过短（<0.5s），面板可能一闪而过"));
			}
		}
		// ---------- 等待确认 ----------
		else if (const UMultipleTask_WaitForConfirm* Confirm = Cast<UMultipleTask_WaitForConfirm>(Task))
		{
			if (Confirm->ConfirmButtonText.IsEmpty())
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc,
					TEXT("ConfirmButtonText 为空，不会生成确认按钮，永远无法完成"));
			}
			if (Confirm->ConfirmPolicy == EConfirmPolicy::SpecificRoles && Confirm->SpecificRoleTags.Num() == 0)
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc,
					TEXT("ConfirmPolicy = SpecificRoles 但 SpecificRoleTags 为空"));
			}
		}
		// ---------- 身份选择 ----------
		else if (const UMultipleTask_RoleSelect* RoleSelect = Cast<UMultipleTask_RoleSelect>(Task))
		{
			if (RoleSelect->Roles.Num() == 0)
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("Roles 为空，玩家无法选择身份"));
			}
			else
			{
				TSet<FName> SeenRoles;
				for (const FRoleDefinition& Role : RoleSelect->Roles)
				{
					if (Role.RoleTag.IsNone())
					{
						AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("Roles 中存在空 RoleTag"));
					}
					else if (SeenRoles.Contains(Role.RoleTag))
					{
						AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
							FString::Printf(TEXT("RoleTag %s 重复"), *Role.RoleTag.ToString()));
					}
					else
					{
						SeenRoles.Add(Role.RoleTag);
					}
				}

				if (RoleSelect->Roles.Num() < 3)
				{
					AddIssue(Issues, ETaskDebugSeverity::Info, Loc,
						TEXT("Roles 少于 3 个，标准流程需要 3 名参训玩家各选一个身份"));
				}
			}
		}
		// ---------- 多人同时到位 ----------
		else if (const UMultipleTask_MultiStationArrival* Arrival = Cast<UMultipleTask_MultiStationArrival>(Task))
		{
			ValidateStationRequirements(World, Arrival->Requirements, Loc, Issues);
		}
		// ---------- 就位确认 ----------
		else if (const UMultipleTask_StationConfirm* StationConfirm = Cast<UMultipleTask_StationConfirm>(Task))
		{
			ValidateStationRequirements(World, StationConfirm->Requirements, Loc, Issues);
			if (StationConfirm->ConfirmButtonText.IsEmpty())
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc,
					TEXT("ConfirmButtonText 为空，面板上没有确认按钮"));
			}
		}
		// ---------- 指引到位置 ----------
		else if (const UMultipleTask_GuideToLocation* Guide = Cast<UMultipleTask_GuideToLocation>(Task))
		{
			if (Guide->StationTag.IsNone())
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("StationTag 为空，任务无法完成"));
			}
			else if (World && !ActorWithTagExists(World, Guide->StationTag))
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
					FString::Printf(TEXT("场景中找不到带 Tag %s 的站位"), *Guide->StationTag.ToString()));
			}

			if (Guide->RequiredRoleTag.IsNone())
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc, TEXT("RequiredRoleTag 为空，任何进入站位的人都会触发"));
			}

			if (Guide->bShowMarker && Guide->MarkerClass.IsNull())
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc, TEXT("bShowMarker = true 但 MarkerClass 为空"));
			}
		}
		// ---------- 装备检查 ----------
		else if (const UMultipleTask_EquipmentInteract* EquipInteract = Cast<UMultipleTask_EquipmentInteract>(Task))
		{
			if (EquipInteract->Requirements.Num() == 0)
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("Requirements 为空，任务无法完成"));
			}
			for (int32 Index = 0; Index < EquipInteract->Requirements.Num(); ++Index)
			{
				if (EquipInteract->Requirements[Index].RequiredTags.Num() == 0)
				{
					AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
						FString::Printf(TEXT("Requirements[%d] 的 RequiredTags 为空"), Index));
				}
			}
			if (EquipInteract->CheckInterval < 0.1f)
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc, TEXT("CheckInterval 小于 0.1 秒，轮询过于频繁"));
			}
		}
		// ---------- 自主选装确认 ----------
		else if (const UMultipleTask_EquipmentConfirm* EquipConfirm = Cast<UMultipleTask_EquipmentConfirm>(Task))
		{
			if (EquipConfirm->StandardEquipment.Num() == 0)
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("StandardEquipment 为空，无法核对装备"));
			}
			if (EquipConfirm->EquipmentTagPool.Num() == 0)
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
					TEXT("EquipmentTagPool 为空，可能把身份等非装备 Tag 误判为装备"));
			}
			for (int32 Index = 0; Index < EquipConfirm->StandardEquipment.Num(); ++Index)
			{
				if (EquipConfirm->StandardEquipment[Index].RequiredTags.Num() == 0)
				{
					AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
						FString::Printf(TEXT("StandardEquipment[%d] 的 RequiredTags 为空"), Index));
				}
			}
		}
		// ---------- 区域监控（含警戒待命 / 安全距离） ----------
		else if (const UMultipleTask_ZoneMonitor* Zone = Cast<UMultipleTask_ZoneMonitor>(Task))
		{
			if (Zone->ZoneActorTag.IsNone())
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc,
					TEXT("ZoneActorTag 为空，任务启动后会立即结束，步骤被跳过"));
			}
			else if (World && !ActorWithTagExists(World, Zone->ZoneActorTag))
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
					FString::Printf(TEXT("场景中找不到带 Tag %s 的区域/嫌疑人"), *Zone->ZoneActorTag.ToString()));
			}

			if (Zone->RequiredCompliantDuration <= 0.f)
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
					TEXT("RequiredCompliantDuration <= 0，必须由外部调用 FinishMonitoring 才会结束"));
			}
			if (Zone->WarningContent.IsEmpty())
			{
				AddIssue(Issues, ETaskDebugSeverity::Info, Loc, TEXT("WarningContent 为空，违规提示没有正文"));
			}
		}
		// ---------- 搜身检查 ----------
		else if (const UMultipleTask_SearchInspection* Search = Cast<UMultipleTask_SearchInspection>(Task))
		{
			if (Search->BodyParts.Num() == 0)
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("BodyParts 为空，任务无法完成"));
			}
			for (int32 Index = 0; Index < Search->BodyParts.Num(); ++Index)
			{
				if (Search->BodyParts[Index].CollisionBoxTag.IsNone())
				{
					AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
						FString::Printf(TEXT("BodyParts[%d] 的 CollisionBoxTag 为空"), Index));
				}
			}

			if (World && !ActorWithTagExists(World, Search->TargetActorTag))
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
					FString::Printf(TEXT("场景中找不到带 Tag %s 的搜身目标"), *Search->TargetActorTag.ToString()));
			}
		}
		// ---------- NPC 行为 ----------
		else if (const UMultipleTask_NPCBehavior* NPC = Cast<UMultipleTask_NPCBehavior>(Task))
		{
			if (NPC->bUseExistingNPC)
			{
				if (NPC->ExistingNPCTag.IsNone())
				{
					AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("接管已有 NPC 但 ExistingNPCTag 为空"));
				}
				else if (World && !ActorWithTagExists(World, NPC->ExistingNPCTag))
				{
					AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
						FString::Printf(TEXT("场景中找不到带 Tag %s 的已有 NPC"), *NPC->ExistingNPCTag.ToString()));
				}
			}
			else if (NPC->NPCClass.IsNull())
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("NPCClass 为空，无法生成 NPC"));
			}

			if (NPC->NPCCount <= 0)
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc, TEXT("NPCCount <= 0"));
			}
			if (NPC->BehaviorSequence.Num() == 0)
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc, TEXT("BehaviorSequence 为空，任务会立即结束"));
			}

			for (int32 Index = 0; Index < NPC->BehaviorSequence.Num(); ++Index)
			{
				const FNPCBehaviorNode& Node = NPC->BehaviorSequence[Index];

				switch (Node.NodeType)
				{
				case ENPCBehaviorType::MoveTo:
					if (Node.TargetLocationTag.IsNone() && Node.TargetLocation.IsNearlyZero())
					{
						AddIssue(Issues, ETaskDebugSeverity::Info, Loc,
							FString::Printf(TEXT("BehaviorSequence[%d] 移动目标为原点且没有 Tag"), Index));
					}
					break;
				case ENPCBehaviorType::Wait:
					if (Node.WaitDuration < 0.f)
					{
						AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
							FString::Printf(TEXT("BehaviorSequence[%d] WaitDuration < 0"), Index));
					}
					break;
				case ENPCBehaviorType::PlayAnim:
					if (!Node.AnimToPlay)
					{
						AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
							FString::Printf(TEXT("BehaviorSequence[%d] 未配置 AnimToPlay，该节点会被跳过"), Index));
					}
					break;
				case ENPCBehaviorType::PlayAudio:
					if (!Node.AudioToPlay)
					{
						AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
							FString::Printf(TEXT("BehaviorSequence[%d] 未配置 AudioToPlay，该节点会被跳过"), Index));
					}
					break;
				case ENPCBehaviorType::LookAt:
					if (Node.LookAtTargetTag.IsNone())
					{
						AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
							FString::Printf(TEXT("BehaviorSequence[%d] LookAtTargetTag 为空"), Index));
					}
					break;
				case ENPCBehaviorType::SetAIState:
					if (Node.AIStateName.IsNone())
					{
						AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
							FString::Printf(TEXT("BehaviorSequence[%d] AIStateName 为空"), Index));
					}
					break;
				default:
					break;
				}
			}
		}
		// ---------- 动画播放 ----------
		else if (const UMultipleTask_PlayAnim* PlayAnim = Cast<UMultipleTask_PlayAnim>(Task))
		{
			if (PlayAnim->bNeedMultipleAnim)
			{
				if (PlayAnim->PlayAnimInfoArray.Num() == 0)
				{
					AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("需要多段动画但 PlayAnimInfoArray 为空"));
				}
				for (int32 Index = 0; Index < PlayAnim->PlayAnimInfoArray.Num(); ++Index)
				{
					if (!PlayAnim->PlayAnimInfoArray[Index].PlayAnimSequence)
					{
						AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
							FString::Printf(TEXT("PlayAnimInfoArray[%d] 未配置动画"), Index));
					}
				}
			}
			else if (!PlayAnim->PlayAnimInfo.PlayAnimSequence)
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("PlayAnimInfo 未配置动画，任务会立即结束"));
			}

			if (PlayAnim->LoopCount < 0)
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc, TEXT("LoopCount < 0"));
			}
			else if (PlayAnim->LoopCount == 0)
			{
				AddIssue(Issues, ETaskDebugSeverity::Info, Loc, TEXT("LoopCount = 0 为无限循环，只能由外部结束"));
			}

			if (PlayAnim->TargetRoleTag.IsNone() && PlayAnim->TargetPawnRef.IsNull())
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc, TEXT("既没有 TargetRoleTag 也没有 TargetPawnRef"));
			}
		}
		// ---------- 音频播放 ----------
		else if (const UMultipleTask_PlayAudio* PlayAudio = Cast<UMultipleTask_PlayAudio>(Task))
		{
			if (PlayAudio->PlayAudioTaskInfo.Num() == 0)
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("PlayAudioTaskInfo 为空，任务会立即结束"));
			}

			for (int32 Index = 0; Index < PlayAudio->PlayAudioTaskInfo.Num(); ++Index)
			{
				const FPlayAudioTaskInfo& Info = PlayAudio->PlayAudioTaskInfo[Index];

				if (!Info.PlaySound)
				{
					AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
						FString::Printf(TEXT("PlayAudioTaskInfo[%d] 未配置 PlaySound"), Index));
				}

				if (Info.PlayAudioType == EAudioPlayLocationType::PlayAudioForPawn
					&& Info.TargetRoleTag.IsNone() && Info.PlayAudioPawn.IsNull())
				{
					AddIssue(Issues, ETaskDebugSeverity::Error, Loc,
						FString::Printf(TEXT("PlayAudioTaskInfo[%d] 按 Pawn 播放但既没有身份也没有 Pawn引用"), Index));
				}

				if (Info.bNeedPlayAnim && !Info.PlayAudioSequence)
				{
					AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
						FString::Printf(TEXT("PlayAudioTaskInfo[%d] 需要动画但 PlayAudioSequence 为空"), Index));
				}

				if (Info.DelayBeforePlayback < 0.f || Info.DelayAfterPlayback < 0.f)
				{
					AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
						FString::Printf(TEXT("PlayAudioTaskInfo[%d] 延迟为负数"), Index));
				}
			}
		}
		// ---------- 语音识别 ----------
		else if (const UMultipleTask_VoiceRecognitionWait* Voice = Cast<UMultipleTask_VoiceRecognitionWait>(Task))
		{
			// 注意：下方提示文案与实际行为相反——MultipleTask_VoiceRecognitionWait 的
			// 匹配逻辑里 RequiredKeyWords 为空意味着"不过滤"，任何语音都会判定通过，
			// 并非"无法通过"。保留原文案仅为兼容既有输出，排查时以本注释为准。
			if (Voice->RequiredKeyWords.Num() == 0)
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("RequiredKeyWords 为空，任何语音都无法通过"));
			}

			for (int32 Index = 0; Index < Voice->RequiredKeyWords.Num(); ++Index)
			{
				if (Voice->RequiredKeyWords[Index].Keyword.IsEmpty())
				{
					AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
						FString::Printf(TEXT("RequiredKeyWords[%d] 的关键词为空"), Index));
				}
			}

			if (Voice->KeywordMatchPolicy == EKeywordMatchPolicy::Ratio && Voice->RequiredRatio <= 0.f)
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc, TEXT("Ratio 模式下 RequiredRatio <= 0"));
			}
			if (Voice->RecognitionTimeout <= 0.f)
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc, TEXT("RecognitionTimeout <= 0，可能立即超时"));
			}
		}
		// ---------- 结束训练 ----------
		else if (const UMultipleTask_EndTraining* EndTraining = Cast<UMultipleTask_EndTraining>(Task))
		{
			if (EndTraining->ReturnToListButtonText.IsEmpty() && EndTraining->RestartButtonText.IsEmpty())
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc, TEXT("两个按钮文字都为空"));
			}
		}
		// ---------- 世界空间 Widget 提示 ----------
		else if (const UMultipleTask_WidgetDisplay* WidgetTask = Cast<UMultipleTask_WidgetDisplay>(Task))
		{
			if (WidgetTask->WidgetClass == nullptr)
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc, TEXT("WidgetClass 为空，不会生成任何提示 Widget"));
			}
			if (WidgetTask->AttachComponentTag.IsNone())
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
					TEXT("AttachComponentTag 为空，Widget 将直接挂在 Pawn 根组件上"));
			}
			if (WidgetTask->Audience == EPanelAudience::SpecificRoles && WidgetTask->TargetRoleTags.Num() == 0)
			{
				AddIssue(Issues, ETaskDebugSeverity::Error, Loc,
					TEXT("Audience = SpecificRoles 但 TargetRoleTags 为空，没有受众"));
			}
			if (!WidgetTask->bDrawAtDesiredSize && (WidgetTask->DrawSize.X <= 0.f || WidgetTask->DrawSize.Y <= 0.f))
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
					TEXT("bDrawAtDesiredSize = false 但 DrawSize 存在非正值，Widget 可能无法显示"));
			}
		}

		// ---------- 错误码 → 评分规则 ----------
		TArray<FName> ErrorCodes;
		if (const UMultipleTask_ZoneMonitor* Zone = Cast<UMultipleTask_ZoneMonitor>(Task))
		{
			ErrorCodes.Add(Zone->ViolationErrorCode);
		}
		if (const UMultipleTask_EquipmentConfirm* EquipConfirm = Cast<UMultipleTask_EquipmentConfirm>(Task))
		{
			ErrorCodes.Add(EquipConfirm->MissingEquipmentErrorCode);
			ErrorCodes.Add(EquipConfirm->WrongEquipmentErrorCode);
		}

		for (FName Code : ErrorCodes)
		{
			if (Code.IsNone())
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc, TEXT("错误码未填写，异常不会扣分"));
			}
			else if (Asset && !HasScoreRule(Asset, Code))
			{
				AddIssue(Issues, ETaskDebugSeverity::Warning, Loc,
					FString::Printf(TEXT("错误码 %s 在全表 ScoreRules 中找不到，异常不会扣分"), *Code.ToString()));
			}
		}
	}

	/** 打印到控制台（ConsoleCommand） */
	void PrintIssuesToConsole(const TArray<FTaskDebugIssue>& Issues, const FString& Title, FOutputDevice& Ar)
	{
		Ar.Logf(TEXT("===== %s ====="), *Title);
		for (const FTaskDebugIssue& Issue : Issues)
		{
			Ar.Logf(TEXT("%s %s | %s"), SeverityPrefix(Issue.Severity), *Issue.Location, *Issue.Message);
		}

		int32 Errors = 0;
		int32 Warnings = 0;
		int32 Infos = 0;
		for (const FTaskDebugIssue& Issue : Issues)
		{
			switch (Issue.Severity)
			{
			case ETaskDebugSeverity::Error:   ++Errors;   break;
			case ETaskDebugSeverity::Warning: ++Warnings; break;
			default:                          ++Infos;    break;
			}
		}
		Ar.Logf(TEXT("汇总：错误 %d，警告 %d，提示 %d"), Errors, Warnings, Infos);
	}
}

// ==================== 校验 ====================

int32 FTaskDebug::ValidateAsset(UWorld* World, const UPrimaryTaskDataAsset* Asset,
	TArray<FTaskDebugIssue>& OutIssues, bool bLogToConsole)
{
	OutIssues.Reset();

	if (!Asset)
	{
		AddIssue(OutIssues, ETaskDebugSeverity::Error, TEXT("Asset"), TEXT("配表为空"));
		if (bLogToConsole)
		{
			PrintIssues(OutIssues, TEXT("配表校验"));
		}
		return 1;
	}

	if (Asset->Steps.Num() == 0)
	{
		AddIssue(OutIssues, ETaskDebugSeverity::Error, TEXT("Asset"),
			TEXT("Steps 为空，任务无法开始"));
	}

	TSet<FName> SeenStepIds;

	for (int32 StepIndex = 0; StepIndex < Asset->Steps.Num(); ++StepIndex)
	{
		const FTaskStep& Step = Asset->Steps[StepIndex];
		const FString StepLoc = MakeStepLocation(StepIndex, Step);

		if (Step.StepId.IsNone())
		{
			AddIssue(OutIssues, ETaskDebugSeverity::Warning, StepLoc, TEXT("StepId 未填写"));
		}
		else if (SeenStepIds.Contains(Step.StepId))
		{
			AddIssue(OutIssues, ETaskDebugSeverity::Warning, StepLoc, TEXT("StepId 重复"));
		}
		else
		{
			SeenStepIds.Add(Step.StepId);
		}

		if (Step.ParallelTasks.Num() == 0)
		{
			AddIssue(OutIssues, ETaskDebugSeverity::Warning, StepLoc,
				TEXT("ParallelTasks 为空，该步骤会被瞬间跳过"));
		}

		int32 PanelDisplayCount = 0;

		for (const TObjectPtr<UMultipleTaskBase>& TaskPtr : Step.ParallelTasks)
		{
			UMultipleTaskBase* Task = TaskPtr;
			if (!Task)
			{
				AddIssue(OutIssues, ETaskDebugSeverity::Error, StepLoc, TEXT("ParallelTasks 中存在空任务项"));
				continue;
			}

			if (Cast<UMultipleTask_UIPanelDisplay>(Task))
			{
				++PanelDisplayCount;
			}

			ValidateTask(World, Asset, StepIndex, &Step, Task, OutIssues);
		}

		if (PanelDisplayCount > 1)
		{
			AddIssue(OutIssues, ETaskDebugSeverity::Warning, StepLoc,
				FString::Printf(TEXT("同一步骤内有 %d 个 UI 展示面板，会互相覆盖"), PanelDisplayCount));
		}
	}

	if (!World)
	{
		AddIssue(OutIssues, ETaskDebugSeverity::Info, TEXT("Asset"),
			TEXT("未提供 World，已跳过场景 Tag / Actor 查找类检查"));
	}

	int32 ErrorCount = 0;
	for (const FTaskDebugIssue& Issue : OutIssues)
	{
		if (Issue.Severity == ETaskDebugSeverity::Error)
		{
			++ErrorCount;
		}
	}

	if (bLogToConsole)
	{
		PrintIssues(OutIssues, FString::Printf(TEXT("配表校验：%s"), *Asset->GetName()));
	}

	return ErrorCount;
}

int32 FTaskDebug::ValidateActiveTasks(UWorld* World, TArray<FTaskDebugIssue>& OutIssues)
{
	OutIssues.Reset();

	UTaskSequencerComponent* Sequencer = FindSequencer(World);
	if (!Sequencer)
	{
		return 0;
	}

	const int32 StepIndex = Sequencer->GetCurrentStepIndex();
	UPrimaryTaskDataAsset* Asset = Sequencer->GetCurrentTaskAssetForDebug();

	for (const TObjectPtr<UMultipleTaskBase>& TaskPtr : Sequencer->GetActiveTasksForDebug())
	{
		UMultipleTaskBase* Task = TaskPtr;
		if (Task && !Task->bTaskEnded)
		{
			ValidateTask(World, Asset, StepIndex, nullptr, Task, OutIssues);
		}
	}

	int32 ErrorCount = 0;
	for (const FTaskDebugIssue& Issue : OutIssues)
	{
		if (Issue.Severity == ETaskDebugSeverity::Error)
		{
			++ErrorCount;
		}
	}
	return ErrorCount;
}

void FTaskDebug::PrintIssues(const TArray<FTaskDebugIssue>& Issues, const FString& Title)
{
	UE_LOG(LogTemp, Display, TEXT("===== %s ====="), *Title);
	for (const FTaskDebugIssue& Issue : Issues)
	{
		UE_LOG(LogTemp, Display, TEXT("%s %s | %s"),
			SeverityPrefix(Issue.Severity), *Issue.Location, *Issue.Message);
	}

	int32 Errors = 0;
	int32 Warnings = 0;
	int32 Infos = 0;
	for (const FTaskDebugIssue& Issue : Issues)
	{
		switch (Issue.Severity)
		{
		case ETaskDebugSeverity::Error:   ++Errors;   break;
		case ETaskDebugSeverity::Warning: ++Warnings; break;
		default:                          ++Infos;    break;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("汇总：错误 %d，警告 %d，提示 %d"), Errors, Warnings, Infos);
}

UTaskSequencerComponent* FTaskDebug::FindSequencer(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->FindComponentByClass<UTaskSequencerComponent>() : nullptr;
}

// ==================== 控制台命令 ====================

namespace
{
	void CmdHelp(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		Ar.Logf(TEXT("===== 任务系统调试命令 ====="));
		Ar.Logf(TEXT("Task.Dump                          打印当前配表结构"));
		Ar.Logf(TEXT("Task.Validate [AssetPath]          自检配表（不填路径则校验当前配表）"));
		Ar.Logf(TEXT("Task.Skip <StepIndex>              跳转到指定步骤"));
		Ar.Logf(TEXT("Task.Finish <ClassName|All>        结束指定类（支持子串）或全部活跃任务"));
		Ar.Logf(TEXT("Task.Restart                       用当前配表重新开始"));
		Ar.Logf(TEXT("Task.Roles                         打印参训玩家与身份分配"));
		Ar.Logf(TEXT("Task.Panels                        打印每个玩家的面板状态"));
		Ar.Logf(TEXT("Task.Scores                        打印评分板"));
		Ar.Logf(TEXT("Task.Debug <0|1>                   场景 Tag / 区域可视化与 HUD 开关"));
	}

	void CmdDump(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UTaskSequencerComponent* Sequencer = FTaskDebug::FindSequencer(World);
		if (!Sequencer)
		{
			Ar.Logf(TEXT("[Task] 找不到 Sequencer（需要在游戏世界中执行）"));
			return;
		}

		UPrimaryTaskDataAsset* Asset = Sequencer->GetCurrentTaskAssetForDebug();
		if (!Asset)
		{
			Ar.Logf(TEXT("[Task] 当前没有加载配表"));
			return;
		}

		Ar.Logf(TEXT("===== %s（共 %d 步）====="), *Asset->TaskName.ToString(), Asset->Steps.Num());
		for (int32 StepIndex = 0; StepIndex < Asset->Steps.Num(); ++StepIndex)
		{
			const FTaskStep& Step = Asset->Steps[StepIndex];
			Ar.Logf(TEXT("[%02d] %s  %s"), StepIndex,
				*Step.StepId.ToString(), *Step.StepTitle.ToString());

			for (const TObjectPtr<UMultipleTaskBase>& TaskPtr : Step.ParallelTasks)
			{
				UMultipleTaskBase* Task = TaskPtr;
				if (!Task)
				{
					Ar.Logf(TEXT("      <空任务项>"));
					continue;
				}
				Ar.Logf(TEXT("      %s  <%s>"), *Task->TaskName.ToString(), *Task->GetClass()->GetName());
			}

			for (const FScoreRule& Rule : Step.ScoreRules)
			{
				Ar.Logf(TEXT("      -%d  %s  (%s)"), Rule.Penalty,
					*Rule.ErrorCode.ToString(), *Rule.Description.ToString());
			}
		}
	}

	void CmdValidate(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UPrimaryTaskDataAsset* Asset = nullptr;

		if (Args.Num() > 0)
		{
			Asset = LoadObject<UPrimaryTaskDataAsset>(nullptr, *Args[0]);
			if (!Asset)
			{
				Ar.Logf(TEXT("[Task] 无法加载配表：%s"), *Args[0]);
				return;
			}
		}
		else
		{
			UTaskSequencerComponent* Sequencer = FTaskDebug::FindSequencer(World);
			Asset = Sequencer ? Sequencer->GetCurrentTaskAssetForDebug() : nullptr;
		}

		if (!Asset)
		{
			Ar.Logf(TEXT("[Task] 没有可校验的配表，可用 Task.Validate /Game/.../DA_XXX 指定路径"));
			return;
		}

		TArray<FTaskDebugIssue> Issues;
		FTaskDebug::ValidateAsset(World, Asset, Issues, false);
		PrintIssuesToConsole(Issues, FString::Printf(TEXT("配表校验：%s"), *Asset->GetName()), Ar);
	}

	void CmdSkip(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UTaskSequencerComponent* Sequencer = FTaskDebug::FindSequencer(World);
		if (!Sequencer)
		{
			Ar.Logf(TEXT("[Task] 找不到 Sequencer（需要在游戏世界中执行）"));
			return;
		}

		if (Args.Num() < 1)
		{
			Ar.Logf(TEXT("用法：Task.Skip <StepIndex>"));
			return;
		}

		const int32 StepIndex = FCString::Atoi(*Args[0]);
		if (StepIndex < 0)
		{
			Ar.Logf(TEXT("[Task] StepIndex 不能为负数"));
			return;
		}

		Sequencer->SkipToStep(StepIndex);
		Ar.Logf(TEXT("[Task] 已请求跳转到 Step %d（当前 Step %d）"),
			StepIndex, Sequencer->GetCurrentStepIndex());
	}

	void CmdFinish(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UTaskSequencerComponent* Sequencer = FTaskDebug::FindSequencer(World);
		if (!Sequencer)
		{
			Ar.Logf(TEXT("[Task] 找不到 Sequencer（需要在游戏世界中执行）"));
			return;
		}

		if (Args.Num() < 1)
		{
			Ar.Logf(TEXT("用法：Task.Finish <ClassName|All>"));
			return;
		}

		// 快照：EndTask 会修改 ActiveTasks
		TArray<UMultipleTaskBase*> Snapshot;
		for (const TObjectPtr<UMultipleTaskBase>& TaskPtr : Sequencer->GetActiveTasksForDebug())
		{
			if (UMultipleTaskBase* Task = TaskPtr)
			{
				Snapshot.Add(Task);
			}
		}

		int32 FinishedCount = 0;
		const bool bFinishAll = Args[0].Equals(TEXT("All"), ESearchCase::IgnoreCase);

		for (UMultipleTaskBase* Task : Snapshot)
		{
			if (!Task || Task->bTaskEnded)
			{
				continue;
			}

			const bool bMatch = bFinishAll
				|| Task->GetClass()->GetName().Contains(Args[0])
				|| Task->TaskName.ToString().Contains(Args[0]);

			if (bMatch)
			{
				Ar.Logf(TEXT("[Task] 结束 %s (%s)"),
					*Task->GetClass()->GetName(), *Task->TaskName.ToString());
				Task->EndTask();
				++FinishedCount;
			}
		}

		if (FinishedCount == 0)
		{
			Ar.Logf(TEXT("[Task] 没有匹配的活跃任务：%s"), *Args[0]);
		}
	}

	void CmdRestart(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UTaskSequencerComponent* Sequencer = FTaskDebug::FindSequencer(World);
		if (!Sequencer)
		{
			Ar.Logf(TEXT("[Task] 找不到 Sequencer（需要在游戏世界中执行）"));
			return;
		}

		UPrimaryTaskDataAsset* Asset = Sequencer->GetCurrentTaskAssetForDebug();
		if (!Asset)
		{
			Ar.Logf(TEXT("[Task] 当前没有加载配表，无法重新开始"));
			return;
		}

		Sequencer->LoadAndStartTask(Asset);
		Ar.Logf(TEXT("[Task] 已用 %s 重新开始训练"), *Asset->GetName());
	}

	void CmdRoles(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UTaskSequencerComponent* Sequencer = FTaskDebug::FindSequencer(World);
		if (!Sequencer)
		{
			Ar.Logf(TEXT("[Task] 找不到 Sequencer（需要在游戏世界中执行）"));
			return;
		}

		const TArray<APlayerController*> Participants = Sequencer->GetParticipants();
		Ar.Logf(TEXT("===== 参训玩家 %d 人 ====="), Participants.Num());
		for (APlayerController* PC : Participants)
		{
			const FName Role = (PC && PC->GetPawn())
				? Sequencer->GetRoleTagByPawn(PC->GetPawn())
				: NAME_None;

			Ar.Logf(TEXT("  %s → %s"),
				PC ? *PC->GetName() : TEXT("<无效Controller>"),
				Role.IsNone() ? TEXT("<未分配>") : *Role.ToString());
		}

		const TArray<FName> Occupied = Sequencer->GetOccupiedRoles();
		FString OccupiedText;
		for (const FName& Role : Occupied)
		{
			OccupiedText += OccupiedText.IsEmpty() ? Role.ToString() : FString::Printf(TEXT(", %s"), *Role.ToString());
		}
		Ar.Logf(TEXT("  已占用身份：%s"), OccupiedText.IsEmpty() ? TEXT("无") : *OccupiedText);
	}

	void CmdPanels(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UTaskSequencerComponent* Sequencer = FTaskDebug::FindSequencer(World);
		if (!Sequencer)
		{
			Ar.Logf(TEXT("[Task] 找不到 Sequencer（需要在游戏世界中执行）"));
			return;
		}

		const TArray<APlayerController*> Participants = Sequencer->GetParticipants();
		Ar.Logf(TEXT("===== 面板状态（%d 人）====="), Participants.Num());

		for (APlayerController* PC : Participants)
		{
			if (!PC)
			{
				continue;
			}

			UTaskPanelChannelComponent* Channel = UTaskPanelChannelComponent::FindChannel(PC);
			if (!Channel)
			{
				Ar.Logf(TEXT("  %s：没有面板通道组件"), *PC->GetName());
				continue;
			}

			const FTaskPanelData& Panel = Channel->GetPanel();
			Ar.Logf(TEXT("  %s：可见=%s  PanelId=%s  PanelType=%d"),
				*PC->GetName(),
				Channel->IsPanelVisible() ? TEXT("是") : TEXT("否"),
				*Panel.PanelId.ToString(),
				(int32)Panel.PanelType);
		}
	}

	void CmdScores(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UScoreComponent* ScoreComponent = UScoreComponent::FindScoreComponent(World);
		if (!ScoreComponent)
		{
			Ar.Logf(TEXT("[Task] 找不到评分组件（GameState 上）"));
			return;
		}

		const TArray<FPlayerScoreEntry> Scores = ScoreComponent->GetAllScores();
		Ar.Logf(TEXT("===== 评分板（%d 条）====="), Scores.Num());

		for (const FPlayerScoreEntry& Entry : Scores)
		{
			FString Codes;
			for (const FName& Code : Entry.ViolatedErrorCodes)
			{
				Codes += Codes.IsEmpty() ? Code.ToString() : FString::Printf(TEXT(", %s"), *Code.ToString());
			}

			Ar.Logf(TEXT("  %s：得分 %d，扣分 %d，异常 %d 次%s%s"),
				*Entry.RoleTag.ToString(), Entry.FinalScore, Entry.TotalDeduction, Entry.ViolationCount,
				Codes.IsEmpty() ? TEXT("") : TEXT("，错误码："),
				*Codes);
		}
	}
}

void FTaskDebug::RegisterConsoleCommands()
{
	static bool bRegistered = false;
	if (bRegistered)
	{
		return;
	}
	bRegistered = true;

	static FAutoConsoleCommandWithWorldArgsAndOutputDevice CmdHelpCommand(
		TEXT("Task.Help"),
		TEXT("列出任务系统调试命令"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CmdHelp));

	static FAutoConsoleCommandWithWorldArgsAndOutputDevice CmdDumpCommand(
		TEXT("Task.Dump"),
		TEXT("打印当前任务配表结构"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CmdDump));

	static FAutoConsoleCommandWithWorldArgsAndOutputDevice CmdValidateCommand(
		TEXT("Task.Validate"),
		TEXT("自检配表：Task.Validate [AssetPath]"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CmdValidate));

	static FAutoConsoleCommandWithWorldArgsAndOutputDevice CmdSkipCommand(
		TEXT("Task.Skip"),
		TEXT("跳转到指定步骤：Task.Skip <StepIndex>"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CmdSkip));

	static FAutoConsoleCommandWithWorldArgsAndOutputDevice CmdFinishCommand(
		TEXT("Task.Finish"),
		TEXT("结束活跃任务：Task.Finish <ClassName|All>"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CmdFinish));

	static FAutoConsoleCommandWithWorldArgsAndOutputDevice CmdRestartCommand(
		TEXT("Task.Restart"),
		TEXT("用当前配表重新开始训练"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CmdRestart));

	static FAutoConsoleCommandWithWorldArgsAndOutputDevice CmdRolesCommand(
		TEXT("Task.Roles"),
		TEXT("打印参训玩家与身份分配"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CmdRoles));

	static FAutoConsoleCommandWithWorldArgsAndOutputDevice CmdPanelsCommand(
		TEXT("Task.Panels"),
		TEXT("打印每个玩家的面板状态"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CmdPanels));

	static FAutoConsoleCommandWithWorldArgsAndOutputDevice CmdScoresCommand(
		TEXT("Task.Scores"),
		TEXT("打印评分板"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CmdScores));
}

// ==================== 可视化 ====================

void FTaskDebugVisualizer::Tick(float DeltaTime)
{
	if (CVarTaskDebug.GetValueOnGameThread() <= 0 || !GEngine)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - LastDrawTime < 0.2)
	{
		return;
	}
	LastDrawTime = Now;

	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* Candidate = Context.World();
		if (Candidate && Candidate->IsGameWorld() && Candidate->GetNetMode() != NM_DedicatedServer)
		{
			World = Candidate;
			break;
		}
	}

	if (!World)
	{
		return;
	}

	UTaskSequencerComponent* Sequencer = FTaskDebug::FindSequencer(World);

	// ---------- Tag 文本 ----------
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->Tags.Num() == 0)
		{
			continue;
		}

		FVector TextLocation = Actor->GetActorLocation() + FVector(0.f, 0.f, 120.f);
		for (const FName& Tag : Actor->Tags)
		{
			DrawDebugString(World, TextLocation, Tag.ToString(), nullptr, FColor::Green, 0.25f, true);
			TextLocation.Z += 18.f;
		}
	}

	// ---------- 当前任务引用的关键 Tag ----------
	TSet<FName> ZoneTags;
	TSet<FName> StationTags;
	TSet<FName> TargetTags;

	if (Sequencer)
	{
		for (const TObjectPtr<UMultipleTaskBase>& TaskPtr : Sequencer->GetActiveTasksForDebug())
		{
			UMultipleTaskBase* Task = TaskPtr;
			if (!Task)
			{
				continue;
			}

			if (const UMultipleTask_ZoneMonitor* Zone = Cast<UMultipleTask_ZoneMonitor>(Task))
			{
				ZoneTags.Add(Zone->ZoneActorTag);
			}
			if (const UMultipleTask_MultiStationArrival* Arrival = Cast<UMultipleTask_MultiStationArrival>(Task))
			{
				for (const FRoleStationPair& Pair : Arrival->Requirements)
				{
					StationTags.Add(Pair.StationTag);
				}
			}
			if (const UMultipleTask_StationConfirm* StationConfirm = Cast<UMultipleTask_StationConfirm>(Task))
			{
				for (const FRoleStationPair& Pair : StationConfirm->Requirements)
				{
					StationTags.Add(Pair.StationTag);
				}
			}
			if (const UMultipleTask_GuideToLocation* Guide = Cast<UMultipleTask_GuideToLocation>(Task))
			{
				StationTags.Add(Guide->StationTag);
			}
			if (const UMultipleTask_SearchInspection* Search = Cast<UMultipleTask_SearchInspection>(Task))
			{
				TargetTags.Add(Search->TargetActorTag);
			}
		}
	}

	TSet<FName> MissingTags;

	auto DrawTaggedBoxes = [&](const TSet<FName>& Tags, const FColor& Color)
	{
		for (const FName& Tag : Tags)
		{
			if (Tag.IsNone())
			{
				continue;
			}

			bool bFound = false;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Actor = *It;
				if (!Actor || !Actor->ActorHasTag(Tag))
				{
					continue;
				}

				FVector Origin;
				FVector Extent;
				Actor->GetActorBounds(false, Origin, Extent);
				DrawDebugBox(World, Origin, Extent, Color, false, 0.25f, 0, 1.5f);
				DrawDebugString(World, Origin + FVector(0.f, 0.f, Extent.Z + 30.f),
					Tag.ToString(), nullptr, Color, 0.25f, true);
				bFound = true;
			}

			if (!bFound)
			{
				MissingTags.Add(Tag);
			}
		}
	};

	DrawTaggedBoxes(ZoneTags, FColor::Green);
	DrawTaggedBoxes(StationTags, FColor::Cyan);
	DrawTaggedBoxes(TargetTags, FColor::Yellow);

	FVector MissingTextLocation(0.f, 0.f, 300.f);
	for (const FName& Tag : MissingTags)
	{
		DrawDebugString(World, MissingTextLocation,
			FString::Printf(TEXT("MISSING TAG: %s"), *Tag.ToString()),
			nullptr, FColor::Red, 0.25f, true);
		MissingTextLocation.Z += 24.f;
	}

	// ---------- 左上角 HUD ----------
	FString Hud;

	if (Sequencer)
	{
		const int32 CurrentStep = Sequencer->GetCurrentStepIndex();
		const int32 TotalSteps = Sequencer->GetTotalStepCount();

		FString StepId;
		FString StepTitle;
		if (UPrimaryTaskDataAsset* Asset = Sequencer->GetCurrentTaskAssetForDebug())
		{
			if (Asset->Steps.IsValidIndex(CurrentStep))
			{
				StepId = Asset->Steps[CurrentStep].StepId.ToString();
				StepTitle = Asset->Steps[CurrentStep].StepTitle.ToString();
			}
		}

		Hud += FString::Printf(TEXT("[Task Debug] Step %d/%d  %s  %s\n"),
			CurrentStep + 1, TotalSteps, *StepId, *StepTitle);

		const TArray<TObjectPtr<UMultipleTaskBase>>& ActiveTasks = Sequencer->GetActiveTasksForDebug();
		for (const TObjectPtr<UMultipleTaskBase>& TaskPtr : ActiveTasks)
		{
			UMultipleTaskBase* Task = TaskPtr;
			if (!Task)
			{
				continue;
			}

			Hud += FString::Printf(TEXT("  %s %s%s\n"),
				Task->bTaskEnded ? TEXT("[√]") : TEXT("[…]"),
				*Task->GetClass()->GetName(),
				Task->TaskName.IsNone() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Task->TaskName.ToString()));
		}

		// 动态计算阻塞任务的完成进度（非阻塞任务不计入）
		int32 BlockingCompleted = 0;
		int32 BlockingTotal = 0;
		for (UMultipleTaskBase* TaskPtr : ActiveTasks)
		{
			UMultipleTaskBase* Task = TaskPtr;
			if (Task && Task->ShouldBlockStepCompletion())
			{
				BlockingTotal++;
				if (Task->bTaskEnded)
				{
					BlockingCompleted++;
				}
			}
		}

		Hud += FString::Printf(TEXT("  Blocking completed %d/%d (total active %d)\n"),
			BlockingCompleted, BlockingTotal, ActiveTasks.Num());

		for (APlayerController* PC : Sequencer->GetParticipants())
		{
			const FName Role = (PC && PC->GetPawn())
				? Sequencer->GetRoleTagByPawn(PC->GetPawn())
				: NAME_None;

			Hud += FString::Printf(TEXT("  %s = %s\n"),
				Role.IsNone() ? TEXT("<未分配>") : *Role.ToString(),
				PC ? *PC->GetName() : TEXT("<无效>"));
		}
	}
	else
	{
		Hud += TEXT("[Task Debug] 没有运行中的 Sequencer\n");
	}

	if (MissingTags.Num() > 0)
	{
		Hud += TEXT("Missing Tags:\n");
		for (const FName& Tag : MissingTags)
		{
			Hud += FString::Printf(TEXT("  %s\n"), *Tag.ToString());
		}
	}

	GEngine->AddOnScreenDebugMessage(0x7A11, 0.3f,
		MissingTags.Num() > 0 ? FColor::Yellow : FColor::White,
		Hud, false, FVector2D(1.1f, 1.1f));
}

bool FTaskDebugVisualizer::IsTickable() const
{
	return CVarTaskDebug.GetValueOnGameThread() > 0;
}

namespace
{
	/** 模块加载时注册控制台命令 */
	struct FTaskDebugRegistrar
	{
		FTaskDebugRegistrar()
		{
			FTaskDebug::RegisterConsoleCommands();
		}
	};

	static FTaskDebugRegistrar GTaskDebugRegistrar;
	static FTaskDebugVisualizer GTaskDebugVisualizer;
}

#endif // !UE_BUILD_SHIPPING
