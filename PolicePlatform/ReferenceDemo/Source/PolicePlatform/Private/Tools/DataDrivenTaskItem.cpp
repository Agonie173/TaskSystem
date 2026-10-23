// Fill out your copyright notice in the Description page of Project Settings.


#include "Tools/DataDrivenTaskItem.h"
#include "UI/CourseTaskWidgetBase.h"// 引入通用任务UI基类
#include "GameFramework/PlayerController.h"// 引入玩家控制器，用于创建视口UI和修改输入模式
#include "GamePlay/PPPlayerBase.h"// 引入项目玩家基类，用于判断当前客户端类型
#include "Camera/PlayerCameraManager.h"// 提供任务阶段相机黑闪
#include "Kismet/GameplayStatics.h"// 提供SpawnSound2D等音频播放函数
#include "Components/AudioComponent.h"// 提供UAudioComponent的停止和销毁功能
#include "UI/CourseTaskWorldWidgetActor.h"// 提供三维任务UI Actor的生成和初始化功能
#include "Engine/World.h"// 提供UWorld和SpawnActor功能
#include "Component/AC_CourseTransition.h"//位置变换组件
#include "Component/AC_Task.h"// 提供任务组件和课程任务开始回调
#include "TimerManager.h"// 提供任务倒计时
#include "UI/CourseSpeechTaskWidgetBase.h"// 提供语音任务专用UI基类
#include "Sound/SoundBase.h"// 提供声音时长读取
#include "Engine/GameInstance.h"// 提供当前客户端GameInstance访问
#include "Tools/InfoSubsystem.h"// 提供当前运行进程类型判断

void UDataDrivenTaskItem::InitializeFromDefinition(const FCourseTaskDefinition& InDefinition)
{
	// 保存数据资产中的完整任务定义，方便后续读取触发方式和事件标识
	Definition = InDefinition;

	// 将任务名称、限时、提示文本、关键词和得分复制到旧任务数据中
	TaskData = InDefinition.TaskData;

	// 设置当前任务是否属于需要记录和上报成绩的关键任务
	IsKeyTask = InDefinition.bIsKeyTask;

	// 设置当前任务是否只允许在训练模式中执行
	bOnlyXunLianModeVaild = InDefinition.bOnlyXunLianModeValid;

	bTestStart = InDefinition.bTestStart;

	// 设置下一项任务的启动间隔，并保证最少延迟0.6秒
	NextTaskInterval = FMath::Max(InDefinition.NextTaskInterval, 0.6f);

	// 判断当前任务是否配置了结束声音
	if (IsValid(InDefinition.CompletionSound))
	{
		// 获取结束声音的播放时长
		const float CompletionSoundDuration = FMath::Max(InDefinition.CompletionSound->GetDuration(), 0.0f);

		// 保证下一任务在结束声音和结束动作完成后再开始
		NextTaskInterval = FMath::Max(NextTaskInterval, CompletionSoundDuration + 0.1f);

		// 输出最终等待时间
		//UE_LOG(LogTemp, Warning, TEXT("任务结束声音时间配置完成：任务=%s，声音时长=%.2f，下一任务等待时间=%.2f"), *InDefinition.TaskData.TaskName, CompletionSoundDuration, NextTaskInterval);
	}

	// 判断当前任务结束后是否需要执行场景过渡
	if (InDefinition.CompletionTransition.bEnabled)
	{
		// 计算画面完全变黑后执行移动的实际等待时间
		const float ActualTransitionDelay = FMath::Max(InDefinition.CompletionTransition.TeleportDelay, InDefinition.CompletionTransition.FadeOutDuration);

		// 取得传送完成后继续保持全黑的安全时间
		const float SafeBlackHoldDuration = FMath::Max(InDefinition.CompletionTransition.BlackHoldDuration, 0.0f);

		// 计算渐黑、移动、全黑保持和渐亮全部完成所需的总时间
		const float RequiredNextTaskInterval = ActualTransitionDelay + SafeBlackHoldDuration + InDefinition.CompletionTransition.FadeInDuration;

		// 保证下一任务不会在场景过渡完成前开始
		NextTaskInterval = FMath::Max(NextTaskInterval, RequiredNextTaskInterval);
	}

	// 初始化任务完成状态，使新创建的任务能够正常响应事件
	bHasCompleted = false;
}

void UDataDrivenTaskItem::StartDataDrivenTask()
{
	// 开始任务前清理上一次运行残留的延迟结束状态
	ResetDataDrivenTaskEndDelay();

	// 清理当前任务可能残留的内容延迟开始状态
	ResetTaskContentStartDelay();

	// 清理当前任务可能残留的阶段黑闪计时器
	ResetTaskStageBlackFadeTimers();


	// 重置任务完成状态，使当前任务可以正常接收新的提交
	bHasCompleted = false;

	// 重置顺序阶段索引，保证重新执行任务时可以重新进入第0阶段
	CurrentTaskStageIndex = INDEX_NONE;

	// 判断任务组件是否有效
	if (IsValid(OwnerAC_Task))
	{
		// 任务内容正式开始前禁止打开装备选择UI
		OwnerAC_Task->SetCourseEquipmentSelectionUIAllowed(false);

		// 任务内容正式开始前禁止使用语音识别
		OwnerAC_Task->SetCourseSpeechRecognitionAllowed(false);

		// 任务内容正式开始前不允许使用PC快捷输入
		OwnerAC_Task->SetActiveCoursePCInputId(NAME_None);

		// 任务内容正式开始前不允许使用VR快捷输入
		OwnerAC_Task->SetActiveCourseVRInputId(NAME_None);

		// 生成任务开始时配置的本地场景Actor
		OwnerAC_Task->SpawnCourseLocalActors(Definition.StartLocalActorSpawns);

		// 执行任务开始时配置的本地Actor通用命令
		OwnerAC_Task->ExecuteCourseLocalActorCommands(Definition.StartLocalActorCommands);

		// 清理上一任务可能残留的本地Actor延时动作
		OwnerAC_Task->ClearCourseLocalActorActionDelayTimers();

		// 立即执行任务开始时配置的本地Actor动作
		OwnerAC_Task->ExecuteCourseLocalActorActions(Definition.StartLocalActorActions);
	}

	// 判断当前任务是否配置了开始场景动作
	if (!Definition.StartActionId.IsNone())
	{
		// 判断任务组件是否有效
		if (IsValid(OwnerAC_Task))
		{
			// 输出任务开始动作日志
			//UE_LOG(LogTemp, Warning, TEXT("课程任务开始动作：任务=%s，动作=%s"), *Definition.TaskData.TaskName, *Definition.StartActionId.ToString());

			// 立即通知蓝图执行当前任务配置的场景动作
			OwnerAC_Task->OnCourseTaskStartAction_BIE(Definition.StartActionId);
		}
		else
		{
			// 输出任务组件无效日志
			UE_LOG(LogTemp, Error, TEXT("课程任务开始动作失败：OwnerAC_Task无效，任务=%s"), *Definition.TaskData.TaskName);
		}
	}

	// 保证任务内容延迟开始时间不会小于零
	const float SafeTaskContentStartDelay = FMath::Max(Definition.TaskContentStartDelay, 0.0f);

	// 没有配置延迟时立即正式开始任务内容
	if (SafeTaskContentStartDelay <= KINDA_SMALL_NUMBER)
	{
		// 立即显示UI、播放旁白并开放任务交互
		StartDataDrivenTaskContent();

		// 当前任务已经完成启动，不再继续创建计时器
		return;
	}

	// 获取当前World
	UWorld* CurrentWorld = GetWorld();

	// World无效时无法创建延迟计时器
	if (!IsValid(CurrentWorld))
	{
		// 输出错误日志并立即开始任务内容，避免任务永久卡住
		UE_LOG(LogTemp, Error, TEXT("课程任务内容延迟开始失败：World无效，将立即开始任务，任务=%s"), *Definition.TaskData.TaskName);

		// World无效时立即正式开始任务内容
		StartDataDrivenTaskContent();

		// 结束当前启动函数
		return;
	}

	// 创建一次性的任务内容延迟开始计时器
	CurrentWorld->GetTimerManager().SetTimer(TaskContentStartDelayTimerHandle, this, &UDataDrivenTaskItem::StartDataDrivenTaskContent, SafeTaskContentStartDelay, false);

	// 输出任务内容进入等待阶段的日志
	//UE_LOG(LogTemp, Warning, TEXT("课程任务已进入内容延迟开始阶段：任务=%s，等待时间=%.2f秒"), *Definition.TaskData.TaskName, SafeTaskContentStartDelay);
}

// 进入指定任务阶段并执行该阶段配置的本地Actor命令
void UDataDrivenTaskItem::EnterTaskStage(int32 StageIndex)
{
	// 只有顺序阶段任务允许进入任务阶段
	if (Definition.TriggerType != ECourseTaskTriggerType::StageSequence)
	{
		return;
	}

	// 防止相同阶段被重复执行
	if (CurrentTaskStageIndex == StageIndex)
	{
		return;
	}

	// 检查阶段索引是否有效
	if (!Definition.TaskStages.IsValidIndex(StageIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("进入任务阶段失败：任务=%s，阶段索引=%d，阶段数量=%d"), *Definition.TaskData.TaskName, StageIndex, Definition.TaskStages.Num());
		return;
	}

	// 保存当前正在执行的阶段索引
	CurrentTaskStageIndex = StageIndex;

	// 取得当前阶段配置
	const FCourseTaskStageDefinition& StageDefinition = Definition.TaskStages[StageIndex];

	// 先输出进入日志，避免Actor命令触发下一阶段后日志顺序看起来相反
	UE_LOG(LogTemp, Warning, TEXT("已进入任务阶段：任务=%s，阶段索引=%d，阶段ID=%s，等待事件=%s，目标=%s"), *Definition.TaskData.TaskName, StageIndex, *StageDefinition.StageId.ToString(), *StageDefinition.EventId.ToString(), *StageDefinition.TargetId.ToString());

	// 当前阶段配置了黑闪时先渐黑，完全黑屏后再执行Actor命令
	if (StageDefinition.bUseBlackFade)
	{
		StartTaskStageBlackFade(StageIndex);
		return;
	}

	// 不需要黑闪时立即执行阶段Actor命令
	ExecuteTaskStageCommands(StageIndex);
}

// 开始指定阶段的黑闪
void UDataDrivenTaskItem::StartTaskStageBlackFade(int32 StageIndex)
{
	// 检查阶段索引是否有效
	if (!Definition.TaskStages.IsValidIndex(StageIndex))
	{
		return;
	}

	// 获取当前阶段配置
	const FCourseTaskStageDefinition& StageDefinition = Definition.TaskStages[StageIndex];

	// 获取当前World
	UWorld* CurrentWorld = GetWorld();

	// World无效时跳过黑闪并立即执行阶段命令
	if (!IsValid(CurrentWorld))
	{
		ExecuteTaskStageCommands(StageIndex);
		return;
	}

	// 清除可能残留的旧黑闪计时器
	ResetTaskStageBlackFadeTimers();

	// 保证渐黑时间有效
	const float SafeFadeOutDuration = FMath::Max(StageDefinition.FadeOutDuration, 0.01f);

	// 本地PC或VR学员使用相机淡出
	if (IsValid(OwnerPlayer) && OwnerPlayer->IsLocallyControlled())
	{
		APlayerController* PlayerController = Cast<APlayerController>(OwnerPlayer->GetController());

		if (IsValid(PlayerController) && IsValid(PlayerController->PlayerCameraManager))
		{
			PlayerController->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, SafeFadeOutDuration, FLinearColor::Black, false, true);
		}
	}

	// 通知蓝图处理像素流端黑色遮罩
	if (IsValid(OwnerAC_Task))
	{
		OwnerAC_Task->OnCourseTaskStageFade_BIE(true, SafeFadeOutDuration);
	}

	// 创建画面完全变黑后的回调
	FTimerDelegate FadeOutFinishedDelegate;
	FadeOutFinishedDelegate.BindUObject(this, &UDataDrivenTaskItem::HandleTaskStageFadeOutFinished, StageIndex);
	CurrentWorld->GetTimerManager().SetTimer(TaskStageFadeOutTimerHandle, FadeOutFinishedDelegate, SafeFadeOutDuration, false);

	UE_LOG(LogTemp, Warning, TEXT("任务阶段开始渐黑：任务=%s，阶段=%s，时间=%.2f"), *Definition.TaskData.TaskName, *StageDefinition.StageId.ToString(), SafeFadeOutDuration);
}

// 画面完全变黑后执行阶段Actor命令
void UDataDrivenTaskItem::HandleTaskStageFadeOutFinished(int32 StageIndex)
{
	// 阶段已经变化时不再执行旧阶段命令
	if (CurrentTaskStageIndex != StageIndex || !Definition.TaskStages.IsValidIndex(StageIndex))
	{
		return;
	}

	// 在全黑状态执行阶段Actor命令
	ExecuteTaskStageCommands(StageIndex);

	// 获取当前阶段配置
	const FCourseTaskStageDefinition& StageDefinition = Definition.TaskStages[StageIndex];

	// 保证全黑保持时间不会小于零
	const float SafeBlackHoldDuration = FMath::Max(StageDefinition.BlackHoldDuration, 0.0f);

	// 不需要保持时立即开始恢复画面
	if (SafeBlackHoldDuration <= KINDA_SMALL_NUMBER)
	{
		FinishTaskStageBlackFade(StageIndex);
		return;
	}

	// 获取当前World
	UWorld* CurrentWorld = GetWorld();

	// World无效时立即恢复画面
	if (!IsValid(CurrentWorld))
	{
		FinishTaskStageBlackFade(StageIndex);
		return;
	}

	// 创建全黑保持结束后的回调
	FTimerDelegate BlackHoldFinishedDelegate;
	BlackHoldFinishedDelegate.BindUObject(this, &UDataDrivenTaskItem::FinishTaskStageBlackFade, StageIndex);
	CurrentWorld->GetTimerManager().SetTimer(TaskStageBlackHoldTimerHandle, BlackHoldFinishedDelegate, SafeBlackHoldDuration, false);
}

// 执行指定阶段配置的Actor命令
void UDataDrivenTaskItem::ExecuteTaskStageCommands(int32 StageIndex)
{
	// 检查阶段索引是否有效
	if (!Definition.TaskStages.IsValidIndex(StageIndex))
	{
		return;
	}

	// 任务组件无效时不能执行Actor命令
	if (!IsValid(OwnerAC_Task))
	{
		UE_LOG(LogTemp, Error, TEXT("执行任务阶段命令失败：OwnerAC_Task无效，任务=%s，阶段索引=%d"), *Definition.TaskData.TaskName, StageIndex);
		return;
	}

	// 执行当前阶段配置的本地Actor命令
	OwnerAC_Task->ExecuteCourseLocalActorCommands(Definition.TaskStages[StageIndex].EnterActorCommands);
}

// 结束黑屏保持并恢复画面
void UDataDrivenTaskItem::FinishTaskStageBlackFade(int32 StageIndex)
{
	// 检查阶段索引是否有效
	if (!Definition.TaskStages.IsValidIndex(StageIndex))
	{
		return;
	}

	// 获取当前阶段配置
	const FCourseTaskStageDefinition& StageDefinition = Definition.TaskStages[StageIndex];

	// 保证渐亮时间有效
	const float SafeFadeInDuration = FMath::Max(StageDefinition.FadeInDuration, 0.01f);

	// 本地PC或VR学员使用相机淡入
	if (IsValid(OwnerPlayer) && OwnerPlayer->IsLocallyControlled())
	{
		APlayerController* PlayerController = Cast<APlayerController>(OwnerPlayer->GetController());

		if (IsValid(PlayerController) && IsValid(PlayerController->PlayerCameraManager))
		{
			PlayerController->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, SafeFadeInDuration, FLinearColor::Black, false, false);
		}
	}

	// 通知蓝图恢复像素流端黑色遮罩
	if (IsValid(OwnerAC_Task))
	{
		OwnerAC_Task->OnCourseTaskStageFade_BIE(false, SafeFadeInDuration);
	}

	UE_LOG(LogTemp, Warning, TEXT("任务阶段开始渐亮：任务=%s，阶段=%s，时间=%.2f"), *Definition.TaskData.TaskName, *StageDefinition.StageId.ToString(), SafeFadeInDuration);
}

// 清理阶段黑闪使用的计时器
void UDataDrivenTaskItem::ResetTaskStageBlackFadeTimers()
{
	UWorld* CurrentWorld = GetWorld();

	if (!IsValid(CurrentWorld))
	{
		return;
	}

	CurrentWorld->GetTimerManager().ClearTimer(TaskStageFadeOutTimerHandle);
	CurrentWorld->GetTimerManager().ClearTimer(TaskStageBlackHoldTimerHandle);
}

void UDataDrivenTaskItem::StartDataDrivenTaskContent()
{
	// 任务内容已经正式开始时不允许重复执行
	if (bHasStartedTaskContent)
	{
		return;
	}

	// 任务已经完成或者正在等待结束时不能再启动任务内容
	if (bHasCompleted || bIsWaitingForCompletionDelay || bHasFinalizedDataDrivenTaskEnd)
	{
		// 输出被阻止的任务内容启动日志
		UE_LOG(LogTemp, Warning, TEXT("课程任务内容启动已忽略：任务已经完成或正在结束，任务=%s"), *Definition.TaskData.TaskName);

		// 结束当前函数
		return;
	}

	// 获取当前World
	UWorld* CurrentWorld = GetWorld();

	// World有效时清理已经到期的任务内容开始计时器
	if (IsValid(CurrentWorld))
	{
		// 清理计时器记录，防止后续错误判断计时器仍然存在
		CurrentWorld->GetTimerManager().ClearTimer(TaskContentStartDelayTimerHandle);
	}

	// 标记当前任务内容已经正式开始
	bHasStartedTaskContent = true;

	// 根据任务配置和当前模式计算最终装备栏权限
	const bool bAllowEquipmentSelectionUI = ShouldAllowEquipmentSelectionUI();

	// 根据当前任务配置开放任务操作权限
	if (IsValid(OwnerAC_Task))
	{
		// 根据配置设置装备选择UI权限
		OwnerAC_Task->SetCourseEquipmentSelectionUIAllowed(bAllowEquipmentSelectionUI);

		// 只有语音关键词任务允许使用语音识别
		OwnerAC_Task->SetCourseSpeechRecognitionAllowed(Definition.TriggerType == ECourseTaskTriggerType::SpeechKeywords);

		// 设置当前任务允许响应的PC快捷输入
		OwnerAC_Task->SetActiveCoursePCInputId(Definition.PCInputActionId);

		// 设置当前任务允许响应的VR快捷输入
		OwnerAC_Task->SetActiveCourseVRInputId(Definition.VRInputActionId);
	}
	else
	{
		// 输出任务组件无效日志
		UE_LOG(LogTemp, Error, TEXT("课程任务内容启动失败：OwnerAC_Task无效，任务=%s"), *Definition.TaskData.TaskName);
	}

	// 任务UI可交互或者允许打开装备栏时，本地PC都需要鼠标和UI输入
	if (Definition.bTaskWidgetInteractive || bAllowEquipmentSelectionUI)
	{
		SetLocalPCCourseUIInputEnabled(true);
	}

	// 根据当前任务配置和运行端类型显示任务UI
	ShowTaskWidget();

	// 播放当前任务配置的旁白
	PlayTaskNarration();

	// 根据任务定义中的触发类型执行不同的任务开始逻辑
	switch (Definition.TriggerType)
	{
	case ECourseTaskTriggerType::None:
		// 没有配置触发类型时只输出日志
		UE_LOG(LogTemp, Warning, TEXT("数据驱动任务内容开始：%s，当前没有配置完成触发方式"), *TaskData.TaskName);
		break;

	case ECourseTaskTriggerType::WaitEvent:
		// 开始等待指定事件和目标物体上报完成消息
		UE_LOG(LogTemp, Warning, TEXT("数据驱动任务内容开始：%s，等待事件=%s，目标=%s"), *TaskData.TaskName, *Definition.EventId.ToString(), *Definition.TargetId.ToString());
		break;

	case ECourseTaskTriggerType::SpeechKeywords:
		// 正式启动服务器权威的语音任务倒计时
		StartSpeechKeywordTask();

		// 输出语音任务开始日志
		UE_LOG(LogTemp, Warning, TEXT("语音关键词任务正式开始：任务=%s，限时=%.1f秒，关键词数量=%d"), *TaskData.TaskName, TaskData.TimeLimit, TaskData.Keywords.Num());
		break;

	case ECourseTaskTriggerType::StageSequence:
		// 顺序阶段任务正式开始时进入第0阶段
		EnterTaskStage(0);
		break;

	case ECourseTaskTriggerType::EquipmentState:
		// 在服务器上立即检查当前学员的课程装备状态
		EvaluateCourseEquipmentStateTask();

		// 当前装备状态任务启动处理完成
		break;

	default:
		// 输出未处理的触发类型
		UE_LOG(LogTemp, Error, TEXT("数据驱动任务内容启动失败：%s，存在未处理的触发类型"), *TaskData.TaskName);
		break;
	}

	// 输出任务内容正式开始日志
	//UE_LOG(LogTemp, Warning, TEXT("课程任务内容正式开始：任务=%s"), *Definition.TaskData.TaskName);
}

// 在服务器上检查当前学员的课程装备状态并结束检查任务
void UDataDrivenTaskItem::EvaluateCourseEquipmentStateTask()
{
	// 任务所属玩家无效时无法判断网络权限
	if (!IsValid(OwnerPlayer))
	{
		UE_LOG(LogTemp, Error, TEXT("课程装备状态检查失败：OwnerPlayer无效，任务=%s"), *Definition.TaskData.TaskName);
		return;
	}

	// 客户端只等待服务器下发最终任务结果
	if (!OwnerPlayer->HasAuthority())
	{
		return;
	}

	// 任务内容尚未开始或任务已经结束时不再重复检查
	if (!CanAcceptCourseSubmission())
	{
		UE_LOG(LogTemp, Warning, TEXT("课程装备状态检查已忽略：任务当前不能接受提交，任务=%s"), *Definition.TaskData.TaskName);
		return;
	}

	// 任务组件无效时按照配置错误结束任务
	if (!IsValid(OwnerAC_Task))
	{
		// 标记当前任务已经完成，防止重复执行
		bHasCompleted = true;

		// 生成配置错误记录
		const FString RecordText = TEXT("课程装备状态检查失败：任务组件无效");

		// 输出任务组件无效日志
		UE_LOG(LogTemp, Error, TEXT("课程装备状态检查失败：OwnerAC_Task无效，任务=%s"), *Definition.TaskData.TaskName);

		// 按失败结果结束当前任务，防止流程永久卡住
		EndTask(ETaskResult::Fail, TEXT(""), RecordText);

		// 结束当前检查函数
		return;
	}

	// 没有配置装备ID时按照配置错误结束任务
	if (Definition.EquipmentStateId.IsNone())
	{
		// 标记当前任务已经完成，防止重复执行
		bHasCompleted = true;

		// 生成配置错误记录
		const FString RecordText = TEXT("课程装备状态检查失败：EquipmentStateId为空");

		// 输出装备ID为空日志
		UE_LOG(LogTemp, Error, TEXT("课程装备状态检查失败：EquipmentStateId为空，任务=%s"), *Definition.TaskData.TaskName);

		// 按失败结果结束当前任务，防止流程永久卡住
		EndTask(ETaskResult::Fail, TEXT(""), RecordText);

		// 结束当前检查函数
		return;
	}

	// 从当前学员自己的任务组件中读取服务器权威装备状态
	const bool bActualEquipmentActive = OwnerAC_Task->IsCourseEquipmentActive(Definition.EquipmentStateId);

	// 判断实际装备状态是否符合任务配置的期望状态
	const bool bEquipmentStateMatched = bActualEquipmentActive == Definition.bExpectedEquipmentActive;

	// 根据装备状态是否符合期望生成任务结果
	const ETaskResult EquipmentTaskResult = bEquipmentStateMatched ? ETaskResult::Succeed : ETaskResult::Fail;

	// 生成人员成绩和日志中使用的装备状态记录
	const FString RecordText = FString::Printf(TEXT("装备状态检查：装备=%s，期望启用=%s，实际启用=%s"), *Definition.EquipmentStateId.ToString(), Definition.bExpectedEquipmentActive ? TEXT("是") : TEXT("否"), bActualEquipmentActive ? TEXT("是") : TEXT("否"));

	// 标记当前装备状态检查任务已经完成
	bHasCompleted = true;

	// 输出服务器最终检查结果
	UE_LOG(LogTemp, Warning, TEXT("课程装备状态检查完成：任务=%s，学员SN=%s，装备=%s，期望启用=%s，实际启用=%s，任务结果=%s"), *Definition.TaskData.TaskName, *OwnerPlayer->GPlayerInfo.SN, *Definition.EquipmentStateId.ToString(), Definition.bExpectedEquipmentActive ? TEXT("是") : TEXT("否"), bActualEquipmentActive ? TEXT("是") : TEXT("否"), *UEnum::GetValueAsString(EquipmentTaskResult));

	// 使用检查结果结束当前任务
	EndTask(EquipmentTaskResult, TEXT(""), RecordText);
}

void UDataDrivenTaskItem::ResetTaskContentStartDelay()
{
	// 获取当前World
	UWorld* CurrentWorld = GetWorld();

	// World有效时清理任务内容延迟开始计时器
	if (IsValid(CurrentWorld))
	{
		// 清除可能残留的内容开始计时器
		CurrentWorld->GetTimerManager().ClearTimer(TaskContentStartDelayTimerHandle);
	}

	// 清除任务内容已经开始的状态
	bHasStartedTaskContent = false;
}

// 根据完整课程事件判断当前任务是否完成，返回当前事件是否成功匹配
bool UDataDrivenTaskItem::TryCompleteByEvent(const FCourseEventData& EventData)
{
	// 延迟开始、重复提交和任务结束阶段都不允许处理课程事件
	if (!CanAcceptCourseSubmission())
	{
		// 输出课程事件被拒绝的日志
		UE_LOG(LogTemp, Warning, TEXT("课程事件已忽略：任务内容尚未开始、已经完成或者正在结束，任务=%s，事件=%s，目标=%s"), *Definition.TaskData.TaskName, *EventData.EventId.ToString(), *EventData.TargetId.ToString());

		// 返回未处理，当前事件不能完成任务
		return false;
	}

	// 判断当前任务是否使用等待事件的触发方式
	if (Definition.TriggerType != ECourseTaskTriggerType::WaitEvent)
	{
		// 返回未处理，表示当前任务不接受课程事件
		return false;
	}

	// 判断上报的事件ID是否与当前任务配置一致
	if (Definition.EventId != EventData.EventId)
	{
		// 返回未处理，表示事件ID不匹配
		return false;
	}

	// 判断当前任务是否配置了目标ID并且上报的目标ID与配置不一致
	if (!Definition.TargetId.IsNone() && Definition.TargetId != EventData.TargetId)
	{
		// 返回未处理，表示目标ID不匹配
		return false;
	}

	// 保存最终匹配当前任务的完整课程事件
	CompletionEventData = EventData;

	// 判断当前任务是否需要根据整数进行评分
	if (Definition.bEvaluateIntegerValue)
	{
		// 判断UI上报的事件是否携带有效整数
		if (!EventData.bHasIntegerValue)
		{
			// 输出错误日志，说明当前评分任务没有收到选择结果
			UE_LOG(LogTemp, Error, TEXT("课程整数事件处理失败：任务=%s，事件没有携带整数"), *Definition.TaskData.TaskName);

			// 返回未处理，保持当前任务继续等待有效选择事件
			return false;
		}

		// 设置任务已经完成，避免结束过程中再次响应按钮事件
		bHasCompleted = true;

		// 生成通用整数状态记录，可用于人数、单选答案和布尔状态评分
		const FString SelectionRecord = FString::Printf(TEXT("任务=%s，实际值=%d，期望值=%d"), *Definition.TaskData.TaskName, EventData.IntegerValue, Definition.ExpectedIntegerValue);

		// 判断玩家选择的整数是否为当前任务的正确答案
		if (EventData.IntegerValue == Definition.ExpectedIntegerValue)
		{
			// 输出正确选择日志，方便检查评分结果
			//UE_LOG(LogTemp, Warning, TEXT("课程整数事件评分成功：任务=%s，玩家选择=%d，正确答案=%d"), *Definition.TaskData.TaskName, EventData.IntegerValue, Definition.ExpectedIntegerValue);

			// 按任务成功结束，旧成绩系统会使用TaskData中的成功分数
			EndTask(ETaskResult::Succeed, TEXT(""), SelectionRecord);

			// 返回处理成功，表示当前事件已经完成任务
			return true;
		}

		// 输出错误选择日志，说明任务将按失败结果结束
		UE_LOG(LogTemp, Warning, TEXT("课程整数事件评分失败：任务=%s，玩家选择=%d，正确答案=%d"), *Definition.TaskData.TaskName, EventData.IntegerValue, Definition.ExpectedIntegerValue);

		// 按任务失败结束，旧成绩系统会使用TaskData中的失败分数
		EndTask(ETaskResult::Fail, TEXT(""), SelectionRecord);

		// 返回处理成功，虽然答案错误，但当前事件已经完成任务
		return true;
	}

	// 普通确认任务匹配成功后设置任务已经完成
	bHasCompleted = true;

	// 输出普通课程事件匹配成功日志
	//UE_LOG(LogTemp, Warning, TEXT("课程事件匹配成功：任务=%s，事件=%s，目标=%s"), *Definition.TaskData.TaskName, *EventData.EventId.ToString(), *EventData.TargetId.ToString());

	// 普通确认任务继续按照成功结果结束
	EndTask(ETaskResult::Succeed, TEXT(""), TEXT(""));

	// 返回处理成功，表示当前普通事件已经完成任务
	return true;
}

// 尝试使用课程事件推进顺序阶段
bool UDataDrivenTaskItem::TryAdvanceByStageEvent(const FCourseEventData& EventData, int32& OutNextStageIndex)
{
	// 默认没有需要进入的下一阶段
	OutNextStageIndex = INDEX_NONE;

	// 任务内容尚未开始、已经完成或正在结束时不能推进阶段
	if (!CanAcceptCourseSubmission())
	{
		UE_LOG(LogTemp, Warning, TEXT("任务阶段事件已忽略：任务当前不能接受提交，任务=%s，事件=%s，目标=%s"), *Definition.TaskData.TaskName, *EventData.EventId.ToString(), *EventData.TargetId.ToString());
		return false;
	}

	// 只有顺序阶段任务允许处理阶段事件
	if (Definition.TriggerType != ECourseTaskTriggerType::StageSequence)
	{
		return false;
	}

	// 检查当前阶段索引是否有效
	if (!Definition.TaskStages.IsValidIndex(CurrentTaskStageIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("任务阶段事件处理失败：当前阶段索引无效，任务=%s，阶段索引=%d，阶段数量=%d"), *Definition.TaskData.TaskName, CurrentTaskStageIndex, Definition.TaskStages.Num());
		return false;
	}

	// 取得当前阶段配置
	const FCourseTaskStageDefinition& CurrentStageDefinition = Definition.TaskStages[CurrentTaskStageIndex];

	// 检查事件ID是否与当前阶段一致
	if (CurrentStageDefinition.EventId != EventData.EventId)
	{
		return false;
	}

	// 当前阶段配置了目标ID时检查目标是否一致
	if (!CurrentStageDefinition.TargetId.IsNone() && CurrentStageDefinition.TargetId != EventData.TargetId)
	{
		return false;
	}

	// 保存本次匹配成功的课程事件
	CompletionEventData = EventData;

	// 计算下一阶段索引
	const int32 NextStageIndex = CurrentTaskStageIndex + 1;

	// 下一阶段存在时等待任务组件同步进入下一阶段
	if (Definition.TaskStages.IsValidIndex(NextStageIndex))
	{
		OutNextStageIndex = NextStageIndex;

		UE_LOG(LogTemp, Warning, TEXT("任务阶段事件匹配成功：任务=%s，当前阶段=%s，下一阶段索引=%d"), *Definition.TaskData.TaskName, *CurrentStageDefinition.StageId.ToString(), NextStageIndex);
		return true;
	}

	// 当前已经是最后一个阶段，标记任务完成
	bHasCompleted = true;

	UE_LOG(LogTemp, Warning, TEXT("顺序阶段任务全部完成：任务=%s，最后阶段=%s"), *Definition.TaskData.TaskName, *CurrentStageDefinition.StageId.ToString());

	// 最后阶段完成后按照成功结果结束整个任务
	EndTask(ETaskResult::Succeed, TEXT(""), TEXT(""));

	return true;
}

// 获取WaitEvent任务或顺序阶段当前等待的事件ID和目标ID
bool UDataDrivenTaskItem::GetCurrentCourseEventCondition(FName& OutEventId, FName& OutTargetId) const
{
	// 默认没有有效完成条件
	OutEventId = NAME_None;
	OutTargetId = NAME_None;

	// 普通等待事件任务直接使用任务配置
	if (Definition.TriggerType == ECourseTaskTriggerType::WaitEvent)
	{
		OutEventId = Definition.EventId;
		OutTargetId = Definition.TargetId;
		return !OutEventId.IsNone();
	}

	// 非顺序阶段任务没有当前阶段完成条件
	if (Definition.TriggerType != ECourseTaskTriggerType::StageSequence)
	{
		return false;
	}

	// 检查当前阶段索引是否有效
	if (!Definition.TaskStages.IsValidIndex(CurrentTaskStageIndex))
	{
		return false;
	}

	// 读取当前阶段的完成条件
	const FCourseTaskStageDefinition& CurrentStageDefinition = Definition.TaskStages[CurrentTaskStageIndex];
	OutEventId = CurrentStageDefinition.EventId;
	OutTargetId = CurrentStageDefinition.TargetId;

	return !OutEventId.IsNone();
}

// 根据DataAsset中的锚点ID在场景中生成三维任务UI
void UDataDrivenTaskItem::ShowWorldTaskWidget()
{
	// 先清理可能残留的三维任务UI，避免任务重复开始时生成多个界面
	RemoveWorldTaskWidget();

	// 判断任务所属玩家是否有效，后续需要从玩家取得当前World
	if (!IsValid(OwnerPlayer))
	{
		// 输出错误日志，说明无法取得生成三维UI需要的World
		UE_LOG(LogTemp, Error, TEXT("三维任务UI生成失败：OwnerPlayer无效，任务=%s"), *Definition.TaskData.TaskName);
		return;
	}

	// 判断负责接收UI确认事件的任务组件是否有效
	if (!IsValid(OwnerAC_Task))
	{
		// 输出错误日志，说明三维UI无法将确认事件发送给任务系统
		UE_LOG(LogTemp, Error, TEXT("三维任务UI生成失败：OwnerAC_Task无效，任务=%s"), *Definition.TaskData.TaskName);
		return;
	}

	// 判断当前任务是否配置了需要显示的Widget类
	if (!Definition.TaskWidgetClass)
	{
		// 输出提示日志，说明当前任务不需要显示三维UI
		UE_LOG(LogTemp, Warning, TEXT("三维任务UI未生成：TaskWidgetClass为空，任务=%s"), *Definition.TaskData.TaskName);
		return;
	}

	// 判断当前任务是否配置了三维UI锚点ID
	if (Definition.WorldWidgetAnchorId.IsNone())
	{
		// 输出错误日志，说明无法确定三维UI应该生成的位置
		UE_LOG(LogTemp, Error, TEXT("三维任务UI生成失败：WorldWidgetAnchorId为空，任务=%s"), *Definition.TaskData.TaskName);
		return;
	}

	// 获取玩家当前所在的World
	UWorld* CurrentWorld = OwnerPlayer->GetWorld();

	// 判断当前World是否有效
	if (!IsValid(CurrentWorld))
	{
		// 输出错误日志，说明当前无法生成Actor
		UE_LOG(LogTemp, Error, TEXT("三维任务UI生成失败：World无效，任务=%s"), *Definition.TaskData.TaskName);
		return;
	}

	// 创建数组，用于接收场景中拥有指定Tag的全部Actor
	TArray<AActor*> FoundAnchorActors;

	// 根据DataAsset中的中文锚点ID查找场景Actor
	UGameplayStatics::GetAllActorsWithTag(CurrentWorld, Definition.WorldWidgetAnchorId, FoundAnchorActors);

	// 判断场景中是否找到了对应的三维UI锚点
	if (FoundAnchorActors.Num() == 0)
	{
		// 输出错误日志，提示检查MainMap中TargetPoint的Actor Tag
		UE_LOG(LogTemp, Error, TEXT("三维任务UI生成失败：没有找到锚点，任务=%s，锚点Tag=%s"), *Definition.TaskData.TaskName, *Definition.WorldWidgetAnchorId.ToString());
		return;
	}

	// 场景中出现同名锚点时输出警告，并继续使用找到的第一个锚点
	if (FoundAnchorActors.Num() > 1)
	{
		// 输出同名锚点数量，方便清理MainMap中的重复锚点
		UE_LOG(LogTemp, Warning, TEXT("三维任务UI找到多个同名锚点：Tag=%s，数量=%d，将使用第一个"), *Definition.WorldWidgetAnchorId.ToString(), FoundAnchorActors.Num());
	}

	// 取得找到的第一个三维UI锚点
	AActor* WorldWidgetAnchor = FoundAnchorActors[0];

	// 判断取得的锚点Actor是否有效
	if (!IsValid(WorldWidgetAnchor))
	{
		// 输出错误日志，说明找到的锚点Actor已经失效
		UE_LOG(LogTemp, Error, TEXT("三维任务UI生成失败：锚点Actor无效，Tag=%s"), *Definition.WorldWidgetAnchorId.ToString());
		return;
	}

	// 配置三维任务UI Actor的生成参数
	FActorSpawnParameters SpawnParameters;

	// 将当前任务所属玩家设置为三维任务UI Actor的Owner
	SpawnParameters.Owner = OwnerPlayer;

	// 忽略生成位置的碰撞，保证三维任务UI能够在锚点位置成功生成
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 在锚点的位置和旋转方向生成三维任务UI Actor
	ActiveWorldWidgetActor = CurrentWorld->SpawnActor<ACourseTaskWorldWidgetActor>(ACourseTaskWorldWidgetActor::StaticClass(), WorldWidgetAnchor->GetActorLocation(), WorldWidgetAnchor->GetActorRotation(), SpawnParameters);

	// 判断三维任务UI Actor是否生成成功
	if (!IsValid(ActiveWorldWidgetActor))
	{
		// 输出错误日志，说明SpawnActor执行失败
		UE_LOG(LogTemp, Error, TEXT("三维任务UI生成失败：无法生成CourseTaskWorldWidgetActor，任务=%s"), *Definition.TaskData.TaskName);
		return;
	}

	// 获取当前实际运行客户端的信息子系统
	UInfoSubsystem* CurrentInfoSubsystem = GetCourseAudioInfoSubsystem();

	// 判断当前是否为像素流客户端
	const bool bCurrentProcessIsViewPort = IsValid(CurrentInfoSubsystem) && CurrentInfoSubsystem->ProcessType == EProcessType::ViewPort;

	// 世界UI使用实际显示端类型，像素流端不能继续使用被观察学员的PC类型
	const EProcessType WorldWidgetProcessType = bCurrentProcessIsViewPort ? EProcessType::ViewPort : OwnerPlayer->ProcessType;

	// 创建世界任务UI并传入文本与运行端类型
	const bool bInitializeSucceeded = ActiveWorldWidgetActor->InitializeWorldWidget(Definition.TaskWidgetClass, OwnerAC_Task, Definition.EventId, Definition.TargetId, Definition.WorldWidgetWidth, OwnerPlayer->bIsXunLianMode, GetTaskDisplayPromptText(), WorldWidgetProcessType);

	// 获取三维UI内部的Widget并初始化语音任务数据
	InitializeSpeechTaskWidget(ActiveWorldWidgetActor->GetCourseTaskWidget());

	// 判断三维任务UI是否初始化成功
	if (!bInitializeSucceeded)
	{
		// 销毁初始化失败的三维任务UI Actor
		ActiveWorldWidgetActor->Destroy();

		// 清空已经失效的三维任务UI Actor引用
		ActiveWorldWidgetActor = nullptr;

		// 输出错误日志，说明Actor已经生成但Widget初始化失败
		UE_LOG(LogTemp, Error, TEXT("三维任务UI生成失败：Widget初始化失败，任务=%s"), *Definition.TaskData.TaskName);
		return;
	}

	// 只有交互类型的世界UI才需要开启射线
	if (Definition.bTaskWidgetInteractive && OwnerPlayer->IsLocallyControlled())
	{
		// 通知当前玩家开启三维UI射线交互
		OwnerPlayer->SetCourseTaskUIInteractionEnabled_BIE(true);
	}

	// 输出成功日志，确认三维任务UI已经生成在指定锚点
	UE_LOG(LogTemp, Warning, TEXT("三维任务UI显示成功：任务=%s，锚点Tag=%s"), *Definition.TaskData.TaskName, *Definition.WorldWidgetAnchorId.ToString());
}

// 销毁当前任务创建的三维任务UI
void UDataDrivenTaskItem::RemoveWorldTaskWidget()
{
	// 只有交互类型的任务UI才需要关闭射线
	if (Definition.bTaskWidgetInteractive && IsValid(OwnerPlayer) && OwnerPlayer->IsLocallyControlled())
	{
		// 通知玩家关闭三维任务UI射线交互
		OwnerPlayer->SetCourseTaskUIInteractionEnabled_BIE(false);
	}

	// 判断当前任务是否存在有效的三维UI Actor
	if (!IsValid(ActiveWorldWidgetActor))
	{
		// 清空可能已经失效的三维UI Actor引用
		ActiveWorldWidgetActor = nullptr;
		return;
	}

	// 销毁当前任务生成的三维UI Actor
	ActiveWorldWidgetActor->Destroy();

	// 清空三维UI Actor引用，避免后续继续访问
	ActiveWorldWidgetActor = nullptr;

	// 输出清理日志，确认任务结束时已经移除三维UI
	//UE_LOG(LogTemp, Warning, TEXT("三维任务UI清理完成：任务=%s"), *Definition.TaskData.TaskName);
}

bool UDataDrivenTaskItem::TryCompleteBySpeechText(const FString& RecognizedText)
{
	// 延迟开始、重复提交和任务结束阶段都不接收语音结果
	if (!CanAcceptCourseSubmission())
	{
		// 输出语音结果被拒绝的日志
		UE_LOG(LogTemp, Warning, TEXT("课程语音结果已忽略：任务内容尚未开始、已经完成或者正在结束，任务=%s"), *Definition.TaskData.TaskName);

		// 返回未处理，当前语音结果不能参与关键词判断
		return false;
	}

	// 只处理语音关键词任务
	if (Definition.TriggerType != ECourseTaskTriggerType::SpeechKeywords)
	{
		return false;
	}

	// 只有服务器负责关键词判断和任务结束
	if (!IsValid(OwnerPlayer) || !OwnerPlayer->HasAuthority())
	{
		return false;
	}

	// 清理识别文本首尾空白
	FString SafeRecognizedText = RecognizedText;
	SafeRecognizedText.TrimStartAndEndInline();

	// 空识别结果不参与判断
	if (SafeRecognizedText.IsEmpty())
	{
		return false;
	}

	// 在多次识别文本之间加入分隔符
	if (!AccumulatedSpeechText.IsEmpty())
	{
		AccumulatedSpeechText.Append(TEXT("；"));
	}

	// 累计本次识别文本
	AccumulatedSpeechText.Append(SafeRecognizedText);

	// 创建去除空白后的匹配文本
	FString ComparableSpeechText = AccumulatedSpeechText;
	ComparableSpeechText.ReplaceInline(TEXT(" "), TEXT(""));
	ComparableSpeechText.ReplaceInline(TEXT("\r"), TEXT(""));
	ComparableSpeechText.ReplaceInline(TEXT("\n"), TEXT(""));

	// 保存有效且不重复的任务关键词
	TSet<FString> RequiredKeywords;

	// 遍历任务配置中的关键词
	for (const FString& ConfiguredKeyword : TaskData.Keywords)
	{
		// 清理关键词首尾空白
		FString SafeKeyword = ConfiguredKeyword;
		SafeKeyword.TrimStartAndEndInline();

		// 忽略空关键词
		if (SafeKeyword.IsEmpty())
		{
			continue;
		}

		// 保存有效关键词
		RequiredKeywords.Add(SafeKeyword);
	}

	// 没有配置有效关键词时不能自动成功
	if (RequiredKeywords.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("语音关键词任务配置错误：没有有效关键词，任务=%s"), *TaskData.TaskName);
		return false;
	}

	// 检查所有需要识别的关键词
	for (const FString& RequiredKeyword : RequiredKeywords)
	{
		// 创建去除空白后的关键词
		FString ComparableKeyword = RequiredKeyword;
		ComparableKeyword.ReplaceInline(TEXT(" "), TEXT(""));

		// 判断累计语音中是否已经包含该关键词
		if (ComparableSpeechText.Contains(ComparableKeyword, ESearchCase::IgnoreCase))
		{
			// 保存已经识别到的关键词
			MatchedSpeechKeywords.Add(RequiredKeyword);
		}
	}

	// 输出当前关键词识别进度
	UE_LOG(LogTemp, Warning, TEXT("服务器收到语音文本：任务=%s，文本=%s，关键词进度=%d/%d"), *TaskData.TaskName, *SafeRecognizedText, MatchedSpeechKeywords.Num(), RequiredKeywords.Num());

	// 关键词还没有全部识别时继续等待
	if (MatchedSpeechKeywords.Num() < RequiredKeywords.Num())
	{
		return false;
	}

	// 标记任务已经完成
	bHasCompleted = true;

	// 清除服务器超时计时器
	if (UWorld* CurrentWorld = GetWorld())
	{
		CurrentWorld->GetTimerManager().ClearTimer(SpeechTaskTimeoutTimerHandle);
	}

	// 生成成绩记录文本
	const FString RecordText = FString::Printf(TEXT("语音关键词识别完成=%d/%d"), MatchedSpeechKeywords.Num(), RequiredKeywords.Num());

	// 按成功结果结束任务
	EndTask(ETaskResult::Succeed, AccumulatedSpeechText, RecordText);

	// 返回任务完成
	return true;
}

bool UDataDrivenTaskItem::CanAcceptCourseSubmission() const
{
	// 任务内容还没有正式开始时拒绝按钮、碰撞、语音和快捷输入提交
	if (!bHasStartedTaskContent)
	{
		return false;
	}

	// 当前任务已经完成时不允许再次提交
	if (bHasCompleted)
	{
		return false;
	}

	// 正在等待任务真正结束时不允许再次提交
	if (bIsWaitingForCompletionDelay)
	{
		return false;
	}

	// 已经执行过最终结束逻辑时不允许再次提交
	if (bHasFinalizedDataDrivenTaskEnd)
	{
		return false;
	}

	// 当前任务内容已经开始并且没有完成，可以正常提交
	return true;
}

// 获取当前客户端的信息子系统
UInfoSubsystem* UDataDrivenTaskItem::GetCourseAudioInfoSubsystem() const
{
	// 任务所属玩家无效时无法取得World
	if (!IsValid(OwnerPlayer))
	{
		return nullptr;
	}

	// 获取任务所属玩家的World
	UWorld* CurrentWorld = OwnerPlayer->GetWorld();

	// World无效时无法取得GameInstance
	if (!IsValid(CurrentWorld))
	{
		return nullptr;
	}

	// 获取当前客户端的GameInstance
	UGameInstance* CurrentGameInstance = CurrentWorld->GetGameInstance();

	// GameInstance无效时无法取得信息子系统
	if (!IsValid(CurrentGameInstance))
	{
		return nullptr;
	}

	// 返回当前客户端的信息子系统
	return CurrentGameInstance->GetSubsystem<UInfoSubsystem>();
}

// 判断当前进程是否需要创建这个学员的任务声音
bool UDataDrivenTaskItem::ShouldCreateCourseTaskAudio() const
{
	// 玩家无效时不能创建声音
	if (!IsValid(OwnerPlayer))
	{
		return false;
	}

	// 获取当前World
	UWorld* CurrentWorld = OwnerPlayer->GetWorld();

	// World无效时不能创建声音
	if (!IsValid(CurrentWorld))
	{
		return false;
	}

	// Dedicated Server不能创建声音
	if (CurrentWorld->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	// 判断任务所属玩家是否为学员
	const bool bOwnerIsStudent = OwnerPlayer->ProcessType == EProcessType::PC_XYD || OwnerPlayer->ProcessType == EProcessType::VR_XYD;

	// 非学员Pawn不能创建课程声音
	if (!bOwnerIsStudent)
	{
		return false;
	}

	// 获取当前客户端的信息子系统
	UInfoSubsystem* InfoSubsystem = GetCourseAudioInfoSubsystem();

	// 像素流客户端需要为全部学员创建声音，以便切换时继续播放
	if (IsValid(InfoSubsystem) && InfoSubsystem->ProcessType == EProcessType::ViewPort)
	{
		return !OwnerPlayer->GPlayerInfo.SN.TrimStartAndEnd().IsEmpty();
	}

	// 获取当前客户端自己的本地玩家控制器
	APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(CurrentWorld, 0);

	// 本地玩家控制器无效时不能播放
	if (!IsValid(LocalPlayerController))
	{
		return false;
	}

	// 学员客户端只允许自己的Pawn创建旁白，禁止播放其他学员旁白
	return LocalPlayerController->GetPawn() == OwnerPlayer;
}

// 获取任务声音当前应使用的音量
float UDataDrivenTaskItem::GetCourseTaskAudioVolume() const
{
	// 获取当前客户端的信息子系统
	UInfoSubsystem* InfoSubsystem = GetCourseAudioInfoSubsystem();

	// 像素流客户端根据当前监控学员SN决定音量
	if (IsValid(InfoSubsystem) &&
		InfoSubsystem->ProcessType == EProcessType::ViewPort &&
		IsValid(OwnerPlayer))
	{
		return InfoSubsystem->GetMonitoredStudentAudioVolume(OwnerPlayer->GPlayerInfo.SN);
	}

	// 普通学员自己的声音正常播放
	return IsValid(OwnerPlayer) && OwnerPlayer->IsLocallyControlled() ? 1.0f : 0.0f;
}

// 将任务声音注册到像素流声音路由
void UDataDrivenTaskItem::RegisterCourseTaskAudio(
	UAudioComponent* AudioComponent) const
{
	// 获取当前客户端的信息子系统
	UInfoSubsystem* InfoSubsystem = GetCourseAudioInfoSubsystem();

	// 子系统、玩家或声音组件无效时不能注册
	if (!IsValid(InfoSubsystem) ||
		!IsValid(OwnerPlayer) ||
		!IsValid(AudioComponent))
	{
		return;
	}

	// 注册声音组件及其所属学员SN
	InfoSubsystem->RegisterMonitoredStudentAudio(
		AudioComponent,
		OwnerPlayer->GPlayerInfo.SN);
}

// 将任务声音从像素流声音路由注销
void UDataDrivenTaskItem::UnregisterCourseTaskAudio(
	UAudioComponent* AudioComponent) const
{
	// 获取当前客户端的信息子系统
	UInfoSubsystem* InfoSubsystem = GetCourseAudioInfoSubsystem();

	// 信息子系统或声音组件无效时不需要处理
	if (!IsValid(InfoSubsystem) || !IsValid(AudioComponent))
	{
		return;
	}

	// 注销已经停止的声音组件
	InfoSubsystem->UnregisterMonitoredStudentAudio(AudioComponent);
}

// 播放当前任务配置的旁白声音
void UDataDrivenTaskItem::PlayTaskNarration()
{
	// 停止可能残留的上一段旁白
	StopTaskNarration();

	// 判断任务是否配置了旁白资源
	if (!IsValid(Definition.NarrationSound))
	{
		//UE_LOG(LogTemp, Warning, TEXT("任务没有配置播报声音：任务=%s"), *Definition.TaskData.TaskName);
		return;
	}

	// 判断任务所属玩家是否有效
	if (!IsValid(OwnerPlayer))
	{
		UE_LOG(LogTemp, Error, TEXT("任务播报播放失败：OwnerPlayer无效，任务=%s"), *Definition.TaskData.TaskName);
		return;
	}

	// 判断当前进程是否需要创建该学员的旁白
	if (!ShouldCreateCourseTaskAudio())
	{
		return;
	}

	// 获取当前客户端的信息子系统
	UInfoSubsystem* InfoSubsystem = GetCourseAudioInfoSubsystem();

	// 判断当前客户端是否为像素流客户端
	const bool bIsViewPortClient = IsValid(InfoSubsystem) && InfoSubsystem->ProcessType == EProcessType::ViewPort;

	// 像素流客户端需要保证零音量声音仍然继续播放
	if (bIsViewPortClient)
	{
		Definition.NarrationSound->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
	}

	// 创建尚未开始播放的二维声音组件
	ActiveNarrationAudio = UGameplayStatics::CreateSound2D(OwnerPlayer, Definition.NarrationSound, 1.0f, 1.0f, 0.0f, nullptr, false, true);

	// 判断声音组件是否创建成功
	if (!IsValid(ActiveNarrationAudio))
	{
		UE_LOG(LogTemp, Error, TEXT("任务播报播放失败：无法创建声音组件，任务=%s"), *Definition.TaskData.TaskName);
		return;
	}

	// 在正式播放前注册声音路由并设置正确音量
	RegisterCourseTaskAudio(ActiveNarrationAudio);

	// 从头开始播放旁白
	ActiveNarrationAudio->Play(0.0f);

	// 获取当前路由音量
	const float RouteVolume = GetCourseTaskAudioVolume();

	// 输出最终播放状态
	/*UE_LOG(
		LogTemp,
		Warning,
		TEXT("任务旁白开始播放：任务=%s，所属SN=%s，声音=%s，路由音量=%.1f，正在播放=%s"),
		*Definition.TaskData.TaskName,
		OwnerPlayer->GPlayerInfo.SN.IsEmpty() ? TEXT("无") : *OwnerPlayer->GPlayerInfo.SN,
		*Definition.NarrationSound->GetName(),
		RouteVolume,
		ActiveNarrationAudio->IsPlaying() ? TEXT("是") : TEXT("否"));*/
}

void UDataDrivenTaskItem::StopTaskNarration()
{
	// 判断当前是否存在有效的播报音频组件
	if (!IsValid(ActiveNarrationAudio))
	{
		// 清空可能已经失效的音频组件引用
		ActiveNarrationAudio = nullptr;
		return;
	}

	// 停止前先从像素流监控声音路由中注销
	UnregisterCourseTaskAudio(ActiveNarrationAudio);

	// 停止当前任务正在播放的播报声音
	ActiveNarrationAudio->Stop();

	// 清空音频组件引用，避免后续继续访问已经停止的组件
	ActiveNarrationAudio = nullptr;

	// 输出停止日志，方便确认任务结束时已经完成声音清理
	//UE_LOG(LogTemp, Warning, TEXT("任务播报停止：任务=%s"), *Definition.TaskData.TaskName);
}

void UDataDrivenTaskItem::PlayTaskCompletionSound()
{
	// 没有配置结束声音时不播放
	if (!IsValid(Definition.CompletionSound))
	{
		return;
	}

	// 玩家无效时无法播放本地声音
	if (!IsValid(OwnerPlayer))
	{
		return;
	}

	// 只有当前进程本地控制的玩家播放声音
	if (!OwnerPlayer->IsLocallyControlled())
	{
		return;
	}

	// 播放任务结束声音
	UGameplayStatics::PlaySound2D(OwnerPlayer, Definition.CompletionSound);

	// 输出播放日志
	//UE_LOG(LogTemp, Warning, TEXT("任务结束声音开始播放：任务=%s，声音=%s"), *TaskData.TaskName, *Definition.CompletionSound->GetName());
}

void UDataDrivenTaskItem::ScheduleTaskCompletionAction()
{
	// 没有配置任务结束动作时不执行
	if (Definition.CompletionActionId.IsNone())
	{
		return;
	}

	// 获取当前世界
	UWorld* CurrentWorld = GetWorld();

	// World无效时直接执行结束动作
	if (!IsValid(CurrentWorld))
	{
		ExecuteTaskCompletionAction();
		return;
	}

	// 清理可能残留的结束动作计时器
	CurrentWorld->GetTimerManager().ClearTimer(CompletionActionTimerHandle);

	// 默认立即执行结束动作
	float CompletionActionDelay = 0.0f;

	// 配置结束声音时等待声音播放完成
	if (IsValid(Definition.CompletionSound))
	{
		CompletionActionDelay = FMath::Max(Definition.CompletionSound->GetDuration(), 0.0f);
	}

	// 没有有效延迟时立即执行
	if (CompletionActionDelay <= KINDA_SMALL_NUMBER)
	{
		ExecuteTaskCompletionAction();
		return;
	}

	// 等待结束声音播放完成后执行场景动作
	CurrentWorld->GetTimerManager().SetTimer(CompletionActionTimerHandle, this, &UDataDrivenTaskItem::ExecuteTaskCompletionAction, CompletionActionDelay, false);

	// 输出动作等待日志
	//UE_LOG(LogTemp, Warning, TEXT("任务结束动作等待执行：任务=%s，动作=%s，等待时间=%.2f"), *TaskData.TaskName, *Definition.CompletionActionId.ToString(), CompletionActionDelay);

}

void UDataDrivenTaskItem::ExecuteTaskCompletionAction()
{
	// 任务组件无效时无法通知蓝图
	if (!IsValid(OwnerAC_Task))
	{
		UE_LOG(LogTemp, Error, TEXT("任务结束动作执行失败：OwnerAC_Task无效，任务=%s"), *TaskData.TaskName);
		return;
	}

	// 输出任务结束动作日志
	//UE_LOG(LogTemp, Warning, TEXT("课程任务结束动作：任务=%s，动作=%s，结果=%s"), *TaskData.TaskName, *Definition.CompletionActionId.ToString(), *UEnum::GetValueAsString(TaskResult.TaskResult));

	// 通知当前客户端蓝图执行场景表现
	OwnerAC_Task->OnCourseTaskCompletionAction_BIE(Definition.CompletionActionId, TaskResult.TaskResult);

}

void UDataDrivenTaskItem::StartSpeechKeywordTask()
{
	// 重置任务完成状态
	bHasCompleted = false;

	// 清空上一轮累计的语音文本
	AccumulatedSpeechText.Empty();

	// 清空上一轮识别成功的关键词
	MatchedSpeechKeywords.Empty();

	// 只有服务器负责最终倒计时和超时结果
	if (!IsValid(OwnerPlayer) || !OwnerPlayer->HasAuthority())
	{
		return;
	}

	// 获取当前世界
	UWorld* CurrentWorld = GetWorld();

	// 世界无效时不能启动倒计时
	if (!IsValid(CurrentWorld))
	{
		UE_LOG(LogTemp, Error, TEXT("语音任务倒计时启动失败：World无效，任务=%s"), *TaskData.TaskName);
		return;
	}

	// 防止错误配置为零或负数
	const float SafeTimeLimit = FMath::Max(TaskData.TimeLimit, 0.1f);

	// 清理可能残留的旧计时器
	CurrentWorld->GetTimerManager().ClearTimer(SpeechTaskTimeoutTimerHandle);

	// 启动服务器权威倒计时
	CurrentWorld->GetTimerManager().SetTimer(SpeechTaskTimeoutTimerHandle, this, &UDataDrivenTaskItem::HandleSpeechTaskTimeout, SafeTimeLimit, false);

	// 输出倒计时日志
	//UE_LOG(LogTemp, Warning, TEXT("服务器启动语音任务倒计时：任务=%s，限时=%.1f秒"), *TaskData.TaskName, SafeTimeLimit);
}

void UDataDrivenTaskItem::HandleSpeechTaskTimeout()
{
	// 任务已经提交成功并进入等待结束阶段时忽略超时回调
	if (!CanAcceptCourseSubmission())
	{
		UE_LOG(LogTemp, Warning, TEXT("课程任务超时回调已忽略：当前任务已经提交成功或正在等待结束"));
		return;
	}

	// 已经成功完成时不再处理超时
	if (bHasCompleted)
	{
		return;
	}

	// 只有服务器允许结束任务
	if (!IsValid(OwnerPlayer) || !OwnerPlayer->HasAuthority())
	{
		return;
	}

	// 标记任务已经结束
	bHasCompleted = true;

	// 生成超时记录文本
	const FString RecordText = FString::Printf(TEXT("语音任务超时，已识别关键词数量=%d"), MatchedSpeechKeywords.Num());

	// 输出超时日志
	UE_LOG(LogTemp, Warning, TEXT("语音关键词任务超时：任务=%s，识别文本=%s"), *TaskData.TaskName, *AccumulatedSpeechText);

	// 按超时结果结束任务
	EndTask(ETaskResult::TimeOut, AccumulatedSpeechText, RecordText);
}

// 获取当前模式和运行端最终需要显示的任务提示文本
FText UDataDrivenTaskItem::GetTaskDisplayPromptText() const
{
	// 语音关键词任务优先使用训练或考核模式文本
	if (Definition.TriggerType == ECourseTaskTriggerType::SpeechKeywords && IsValid(OwnerPlayer))
	{
		// 根据当前训练模式取得对应的语音提示文本
		const FString& DisplayText = OwnerPlayer->bIsXunLianMode ? TaskData.XunLianUIText : TaskData.KaoHeUIText;

		// 转换成UI需要的文本类型
		return FText::FromString(DisplayText);
	}

	// 玩家无效时直接使用通用提示文本
	if (!IsValid(OwnerPlayer))
	{
		return Definition.PromptText;
	}

	// PC学员端优先使用PC专用提示文本
	if (OwnerPlayer->ProcessType == EProcessType::PC_XYD && !Definition.PCPromptTextOverride.IsEmpty())
	{
		return Definition.PCPromptTextOverride;
	}

	// VR学员端和像素流观察端优先使用VR专用提示文本
	if ((OwnerPlayer->ProcessType == EProcessType::VR_XYD || OwnerPlayer->ProcessType == EProcessType::ViewPort) && !Definition.VRPromptTextOverride.IsEmpty())
	{
		return Definition.VRPromptTextOverride;
	}

	// 没有配置运行端专用文本时使用通用提示文本
	return Definition.PromptText;
}

void UDataDrivenTaskItem::FinalizeDataDrivenTaskEnd()
{
	// 检查是否已经真正结束过
	if (bHasFinalizedDataDrivenTaskEnd)
	{
		return;
	}

	// 获取当前World
	UWorld* CompletionWorld = GetWorld();

	// 清除延迟结束计时器
	if (IsValid(CompletionWorld))
	{
		CompletionWorld->GetTimerManager().ClearTimer(CompletionDelayTimerHandle);
	}

	// 停止仍然正在播放的等待阶段声音
	if (IsValid(CompletionDelayAudioComponent))
	{
		// 从像素流学员声音路由中注销
		UnregisterCourseTaskAudio(CompletionDelayAudioComponent);

		// 停止警笛声音
		CompletionDelayAudioComponent->Stop();
	}

	// 清空等待阶段声音引用
	CompletionDelayAudioComponent = nullptr;

	// 关闭正在等待结束状态
	bIsWaitingForCompletionDelay = false;

	// 标记真正结束逻辑已经执行
	bHasFinalizedDataDrivenTaskEnd = true;

	// 清理当前任务可能存在的语音超时计时器
	if (UWorld* CurrentWorld = GetWorld())
	{
		CurrentWorld->GetTimerManager().ClearTimer(SpeechTaskTimeoutTimerHandle);
	}

	// 输出任务结束清理日志，确认当前客户端是否执行了这个函数
	//UE_LOG(LogTemp, Warning, TEXT("进入数据驱动任务结束清理：任务=%s，本地玩家=%s，三维UI=%s"), *Definition.TaskData.TaskName, IsValid(OwnerPlayer) && OwnerPlayer->IsLocallyControlled() ? TEXT("是") : TEXT("否"), *GetNameSafe(ActiveWorldWidgetActor));

	// 当前任务结束后立即禁止继续打开装备选择UI
	if (IsValid(OwnerAC_Task))
	{
		// 禁止继续打开装备选择UI
		OwnerAC_Task->SetCourseEquipmentSelectionUIAllowed(false);

		// 禁止继续录音，并通知蓝图停止当前录音
		OwnerAC_Task->SetCourseSpeechRecognitionAllowed(false);

		// 关闭上一任务的PC快捷输入权限
		OwnerAC_Task->SetActiveCoursePCInputId(NAME_None);

		// 执行任务真正结束时配置的本地Actor通用命令
		OwnerAC_Task->ExecuteCourseLocalActorCommands(Definition.CompletionLocalActorCommands);

		// 为任务所属学员和像素流客户端生成任务结束场景Actor
		OwnerAC_Task->SpawnCourseLocalActors(Definition.CompletionLocalActorSpawns);

		// 关闭上一任务的VR输入权限
		OwnerAC_Task->SetActiveCourseVRInputId(NAME_None);

		// 遍历任务开始时生成的本地Actor
		for (const FCourseLocalActorSpawnDefinition& SpawnDefinition : Definition.StartLocalActorSpawns)
		{
			// 只清理配置为随任务结束销毁的Actor
			if (SpawnDefinition.bDestroyWhenTaskEnds && IsValid(OwnerAC_Task))
			{
				OwnerAC_Task->RemoveCourseLocalActor(SpawnDefinition.ActorId);
			}
		}
	}

	// 判断当前任务结束后是否需要执行场景过渡
	if (Definition.CompletionTransition.bEnabled)
	{
		// 判断任务所属玩家是否有效
		if (!IsValid(OwnerPlayer))
		{
			// 输出玩家无效日志
			UE_LOG(LogTemp, Error, TEXT("任务结束场景过渡失败：OwnerPlayer无效，任务=%s"), *Definition.TaskData.TaskName);
		}
		else if (!IsValid(OwnerPlayer->AC_CourseTransition))
		{
			// 输出课程过渡组件无效日志
			UE_LOG(LogTemp, Error, TEXT("任务结束场景过渡失败：AC_CourseTransition无效，任务=%s"), *Definition.TaskData.TaskName);
		}
		else
		{
			// 根据当前任务的数据配置开始黑屏和场景移动
			OwnerPlayer->AC_CourseTransition->StartCourseTransition(Definition.CompletionTransition);
		}
	}

	// 停止当前任务仍在播放的播报声音
	StopTaskNarration();

	// 播放任务结束时配置的回复声音
	PlayTaskCompletionSound();

	// 等待回复声音结束后执行隐藏虚拟人等场景动作
	ScheduleTaskCompletionAction();

	// 清理VR或像素流使用的三维任务UI
	RemoveWorldTaskWidget();

	// 当前任务创建了UI时将它从视口或三维UI容器中移除
	if (IsValid(ActiveTaskWidget))
	{
		ActiveTaskWidget->RemoveFromParent();
		ActiveTaskWidget = nullptr;
	}

	// 当前任务所属玩家无效时不继续恢复输入
	if (!IsValid(OwnerPlayer))
	{
		return;
	}

	// 只有当前客户端自己控制的PC学员需要恢复输入模式
	if (OwnerPlayer->ProcessType != EProcessType::PC_XYD || !OwnerPlayer->IsLocallyControlled())
	{
		return;
	}

	// 获取当前本地玩家的PlayerController
	APlayerController* PlayerController = Cast<APlayerController>(OwnerPlayer->GetController());

	// 防止操作到服务器控制器或者其他非本地控制器
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	// 判断本任务在当前模式下是否使用过本地PC界面输入
	const bool bUsedLocalPCUIInput = Definition.bTaskWidgetInteractive || ShouldAllowEquipmentSelectionUI();

	if (bUsedLocalPCUIInput)
	{
		SetLocalPCCourseUIInputEnabled(false);
	}

	// 输出任务UI清理完成日志
	UE_LOG(LogTemp, Warning, TEXT("任务UI清理完成：任务=%s"), *TaskData.TaskName);
}

// 重置并清理任务延迟结束状态
void UDataDrivenTaskItem::ResetDataDrivenTaskEndDelay()
{
	// 获取当前World
	UWorld* CurrentWorld = GetWorld();

	// World有效时清理延迟结束计时器
	if (IsValid(CurrentWorld))
	{
		CurrentWorld->GetTimerManager().ClearTimer(CompletionDelayTimerHandle);
	}

	// 停止仍然正在播放的等待阶段声音
	if (IsValid(CompletionDelayAudioComponent))
	{
		// 从像素流学员声音路由中注销
		UnregisterCourseTaskAudio(CompletionDelayAudioComponent);

		// 停止警笛声音
		CompletionDelayAudioComponent->Stop();
	}

	// 清空音频组件引用
	CompletionDelayAudioComponent = nullptr;

	// 清除正在等待结束状态
	bIsWaitingForCompletionDelay = false;

	// 清除已经真正结束状态，允许本任务对象重新开始
	bHasFinalizedDataDrivenTaskEnd = false;
}

// 判断指定功能是否允许在当前训练模式中使用
bool UDataDrivenTaskItem::IsAvailableInCurrentCourseMode(ECourseModeAvailability Availability) const
{
	if (!IsValid(OwnerPlayer))
	{
		return false;
	}

	switch (Availability)
	{
	case ECourseModeAvailability::BothModes:
		return true;

	case ECourseModeAvailability::TrainingOnly:
		return OwnerPlayer->bIsXunLianMode;

	case ECourseModeAvailability::AssessmentOnly:
		return !OwnerPlayer->bIsXunLianMode;

	default:
		return false;
	}
}

// 判断当前任务和模式最终是否允许打开装备选择UI
bool UDataDrivenTaskItem::ShouldAllowEquipmentSelectionUI() const
{
	return Definition.bAllowEquipmentSelectionUI && IsAvailableInCurrentCourseMode(Definition.EquipmentSelectionAvailability);
}

// 为当前任务所属的本地PC学员开启或关闭课程UI输入
void UDataDrivenTaskItem::SetLocalPCCourseUIInputEnabled(bool bEnabled)
{
	// 只有任务所属的本地PC学员允许修改输入模式
	if (!IsValid(OwnerPlayer) || OwnerPlayer->ProcessType != EProcessType::PC_XYD || !OwnerPlayer->IsLocallyControlled())
	{
		return;
	}

	// 获取当前学员自己的本地PlayerController
	APlayerController* PlayerController = Cast<APlayerController>(OwnerPlayer->GetController());

	// 防止修改服务器控制器或其他玩家控制器
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	// 开启课程UI输入
	if (bEnabled)
	{
		// 创建允许游戏和UI同时接收输入的模式
		FInputModeGameAndUI InputMode;

		// 不把鼠标锁定在游戏视口中
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

		// 鼠标点击时不隐藏光标
		InputMode.SetHideCursorDuringCapture(false);

		// 应用游戏和UI混合输入模式
		PlayerController->SetInputMode(InputMode);

		// 显示PC鼠标
		PlayerController->bShowMouseCursor = true;
	}
	else
	{
		// 恢复纯游戏输入模式
		PlayerController->SetInputMode(FInputModeGameOnly());

		// 隐藏PC鼠标
		PlayerController->bShowMouseCursor = false;
	}

	// 输出当前课程UI输入状态
	//UE_LOG(LogTemp, Warning, TEXT("PC课程UI输入状态变化：任务=%s，启用=%s，任务UI可交互=%s，允许装备栏=%s"), *Definition.TaskData.TaskName, bEnabled ? TEXT("是") : TEXT("否"), Definition.bTaskWidgetInteractive ? TEXT("是") : TEXT("否"), Definition.bAllowEquipmentSelectionUI ? TEXT("是") : TEXT("否"));
}

void UDataDrivenTaskItem::ShowTaskWidget()
{
	// 当前任务没有配置Widget类时不创建任务UI
	if (!Definition.TaskWidgetClass)
	{
		return;
	}

	// 当前任务所属玩家无效时无法判断UI显示方式
	if (!IsValid(OwnerPlayer))
	{
		UE_LOG(LogTemp, Error, TEXT("显示任务UI失败：OwnerPlayer无效"));
		return;
	}

	//// 当前玩家不是本地控制玩家时暂时不创建视口UI
	//if (!OwnerPlayer->IsLocallyControlled())
	//{
	//	return;
	//}

	//// 本地VR和像素流客户端需要使用三维UI，下一步单独实现
	//if (OwnerPlayer->ProcessType == EProcessType::VR_XYD || OwnerPlayer->ProcessType == EProcessType::ViewPort)
	//{
	//	// 在DataAsset配置的场景锚点位置生成三维任务UI
	//	ShowWorldTaskWidget();

	//	// 三维任务UI已经处理完成，不再继续创建本地视口UI
	//	return;
	//}

	// 获取当前任务所在的World
	UWorld* CurrentWorld = OwnerPlayer->GetWorld();

	// 获取当前客户端的GameInstance
	UGameInstance* CurrentGameInstance = IsValid(CurrentWorld) ? CurrentWorld->GetGameInstance() : nullptr;

	// 获取当前客户端的信息子系统
	UInfoSubsystem* InfoSubsystem = IsValid(CurrentGameInstance) ? CurrentGameInstance->GetSubsystem<UInfoSubsystem>() : nullptr;

	// 判断运行当前代码的客户端是否为像素流客户端
	const bool bCurrentProcessIsViewPort = IsValid(InfoSubsystem) && InfoSubsystem->ProcessType == EProcessType::ViewPort;

	// 判断当前任务所属学员是否为当前客户端自己控制的玩家
	const bool bOwnerIsLocallyControlled = OwnerPlayer->IsLocallyControlled();

	// 只允许学员自己的客户端和像素流客户端创建任务UI
	if (!bOwnerIsLocallyControlled && !bCurrentProcessIsViewPort)
	{
		return;
	}

	// VR学员本地和像素流客户端统一使用三维任务UI
	if (OwnerPlayer->ProcessType == EProcessType::VR_XYD || bCurrentProcessIsViewPort)
	{
		// 在DataAsset配置的场景锚点位置生成三维任务UI
		ShowWorldTaskWidget();

		// 三维任务UI已经处理完成，不再创建PC视口UI
		return;
	}

	// 只有当前客户端自己控制的PC学员允许修改视口输入模式
	if (OwnerPlayer->ProcessType != EProcessType::PC_XYD || !OwnerPlayer->IsLocallyControlled())
	{
		return;
	}

	// 获取当前本地玩家的PlayerController
	APlayerController* PlayerController = Cast<APlayerController>(OwnerPlayer->GetController());

	// 必须是当前客户端自己的本地PlayerController
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		UE_LOG(LogTemp, Error, TEXT("显示任务UI失败：没有取得本地PC PlayerController，任务=%s，学员=%s"), *TaskData.TaskName, *GetNameSafe(OwnerPlayer));
		return;
	}

	// 根据DataAsset中配置的Widget类创建UI实例
	UUserWidget* CreatedWidget = CreateWidget<UUserWidget>(PlayerController, Definition.TaskWidgetClass);

	// 将创建的UI转换成通用任务UI基类
	ActiveTaskWidget = Cast<UCourseTaskWidgetBase>(CreatedWidget);

	// Widget没有继承CourseTaskWidgetBase时无法使用数据驱动确认流程
	if (!IsValid(ActiveTaskWidget))
	{
		UE_LOG(LogTemp, Error, TEXT("显示任务UI失败：%s没有继承CourseTaskWidgetBase"), *GetNameSafe(Definition.TaskWidgetClass));
		return;
	}

	// 将文本和运行端类型传给PC视口任务UI
	ActiveTaskWidget->InitializeCourseTaskWidget(OwnerAC_Task, Definition.EventId, Definition.TargetId, OwnerPlayer->bIsXunLianMode, GetTaskDisplayPromptText(), OwnerPlayer->ProcessType);

	// 如果当前是语音任务，则继续初始化倒计时和关键词
	InitializeSpeechTaskWidget(ActiveTaskWidget);

	// 将本地PC任务UI添加到玩家视口
	ActiveTaskWidget->AddToViewport();

	// 输出视口UI创建成功日志
	//UE_LOG(LogTemp, Warning, TEXT("本地PC任务UI显示成功：任务=%s，UI=%s"), *TaskData.TaskName, *ActiveTaskWidget->GetName());
}

void UDataDrivenTaskItem::InitializeSpeechTaskWidget(UCourseTaskWidgetBase* TaskWidget)
{
	// 普通任务不需要语音UI初始化
	if (Definition.TriggerType != ECourseTaskTriggerType::SpeechKeywords)
	{
		return;
	}

	// 转换成语音任务UI基类
	UCourseSpeechTaskWidgetBase* SpeechTaskWidget = Cast<UCourseSpeechTaskWidgetBase>(TaskWidget);

	// 检查语音UI是否使用了正确父类
	if (!IsValid(SpeechTaskWidget))
	{
		UE_LOG(LogTemp, Error, TEXT("语音任务UI初始化失败：UI没有继承CourseSpeechTaskWidgetBase，任务=%s"), *TaskData.TaskName);
		return;
	}

	// 将限时和关键词传给语音任务UI
	SpeechTaskWidget->InitializeCourseSpeechTaskWidget(TaskData.TimeLimit, TaskData.Keywords);
}

// 接收到任务结束请求后，根据配置决定立即结束还是延迟结束
void UDataDrivenTaskItem::EndDataDrivenTask()
{
	// 已经真正结束时不能重复执行
	if (bHasFinalizedDataDrivenTaskEnd)
	{
		return;
	}

	// 已经进入等待结束阶段时不能重复创建计时器
	if (bIsWaitingForCompletionDelay)
	{
		return;
	}

	// 清理尚未执行的任务内容延迟开始计时器
	ResetTaskContentStartDelay();

	// 标记当前任务已经进入等待结束阶段
	bIsWaitingForCompletionDelay = true;

	// 任务提交成功后立即关闭当前任务的全部操作权限
	if (IsValid(OwnerAC_Task))
	{
		// 禁止继续打开装备选择UI
		OwnerAC_Task->SetCourseEquipmentSelectionUIAllowed(false);

		// 禁止继续录音，并通知本地蓝图停止当前录音
		OwnerAC_Task->SetCourseSpeechRecognitionAllowed(false);

		// 关闭当前任务的PC快捷输入权限
		OwnerAC_Task->SetActiveCoursePCInputId(NAME_None);

		// 关闭当前任务的VR快捷输入权限
		OwnerAC_Task->SetActiveCourseVRInputId(NAME_None);
	}

	// 只有学员自己的客户端和像素流客户端需要创建该学员的等待阶段声音
	if (IsValid(Definition.CompletionDelaySound.Get()) && ShouldCreateCourseTaskAudio())
	{
		// 获取当前客户端的信息子系统
		UInfoSubsystem* InfoSubsystem = GetCourseAudioInfoSubsystem();

		// 判断当前是否为像素流客户端
		const bool bIsViewPortClient = IsValid(InfoSubsystem) && InfoSubsystem->ProcessType == EProcessType::ViewPort;

		// 像素流端零音量时仍然保持声音播放进度
		if (bIsViewPortClient)
		{
			Definition.CompletionDelaySound->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
		}

		// 创建暂不自动播放的二维声音组件
		CompletionDelayAudioComponent = UGameplayStatics::CreateSound2D(OwnerPlayer, Definition.CompletionDelaySound.Get(), 1.0f, 1.0f, 0.0f, nullptr, false, true);

		// 创建成功后先注册学员声音路由再播放
		if (IsValid(CompletionDelayAudioComponent))
		{
			RegisterCourseTaskAudio(CompletionDelayAudioComponent);
			CompletionDelayAudioComponent->Play(0.0f);
		}
	}

	// 保证等待时间不会小于零
	const float SafeCompletionDelay = FMath::Max(Definition.CompletionDelay, 0.0f);

	// 没有配置等待时间时立即真正结束任务
	if (SafeCompletionDelay <= KINDA_SMALL_NUMBER)
	{
		FinalizeDataDrivenTaskEnd();
		return;
	}

	// 获取当前World
	UWorld* CurrentWorld = GetWorld();

	// World无效时不能创建计时器，直接执行结束避免任务卡死
	if (!IsValid(CurrentWorld))
	{
		UE_LOG(LogTemp, Error, TEXT("课程任务延迟结束失败：World无效，将立即结束任务"));
		FinalizeDataDrivenTaskEnd();
		return;
	}

	// 创建任务延迟结束计时器
	CurrentWorld->GetTimerManager().SetTimer(CompletionDelayTimerHandle, this, &UDataDrivenTaskItem::FinalizeDataDrivenTaskEnd, SafeCompletionDelay, false);

	// 输出任务进入等待结束阶段日志
	UE_LOG(LogTemp, Warning, TEXT("课程任务进入等待结束阶段：等待时间=%.2f秒，等待结束后执行场景过渡"), SafeCompletionDelay);

}
