// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_WidgetDisplay.h"
#include "MultipleTask/UI/TaskWorldWidgetComponent.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMultipleTask_WidgetDisplay::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	UE_LOG(LogTemp, Log, TEXT("[WidgetDisplay] Task start: Widget=%s, AttachTag=%s, Audience=%d, Duration=%.1f"),
		WidgetClass ? *WidgetClass->GetName() : TEXT("None"),
		*AttachComponentTag.ToString(), (int32)Audience, DisplayDuration);

	const TArray<APawn*> TargetPawns = ResolveTargetPawns();
	if (TargetPawns.Num() == 0)
	{
		// 没有受众时直接结束（非阻塞任务，不影响步骤推进）
		UE_LOG(LogTemp, Warning, TEXT("[WidgetDisplay] No target pawn resolved, task ends immediately"));
		EndTask();
		return;
	}

	int32 Created = 0;
	for (APawn* Pawn : TargetPawns)
	{
		USceneComponent* AttachTarget = FindAttachTarget(Pawn);
		if (!Pawn || !AttachTarget)
		{
			continue;
		}

		UTaskWorldWidgetComponent* WidgetComp = NewObject<UTaskWorldWidgetComponent>(Pawn);
		if (!WidgetComp)
		{
			continue;
		}

		// 未注册前用 SetupAttachment 建立父子关系，Register 时按此挂载（相对变换归零）
		WidgetComp->SetupAttachment(AttachTarget);
		Pawn->AddInstanceComponent(WidgetComp);
		// 绘制配置与 Widget 类都在 Register 前设置好（渲染状态按这些值创建）
		WidgetComp->SetReplicatedDrawConfig(DrawSize, bDrawAtDesiredSize);
		WidgetComp->SetReplicatedWidgetClass(WidgetClass);
		WidgetComp->RegisterComponent();

		CreatedWidgets.Add(WidgetComp);
		++Created;

		UE_LOG(LogTemp, Log, TEXT("[WidgetDisplay] Attached widget to pawn %s (target=%s)"),
			*Pawn->GetName(), *AttachTarget->GetName());
	}

	UE_LOG(LogTemp, Log, TEXT("[WidgetDisplay] Created %d widget component(s) on %d target pawn(s)"),
		Created, TargetPawns.Num());

	// 显示时长 >0 时定时结束；=0 时跟随步骤生命周期（Sequencer 推进步骤时 StopAllTasks 触发 EndTask）
	if (DisplayDuration > 0.f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(DisplayTimerHandle, this, &UMultipleTask_WidgetDisplay::EndTask,
				DisplayDuration, false);

			UE_LOG(LogTemp, Log, TEXT("[WidgetDisplay] Auto end scheduled in %.1f s"), DisplayDuration);
		}
	}
}

void UMultipleTask_WidgetDisplay::EndTask_Implementation()
{
	UWorld* World = GetWorld();
	if (World)
	{
		// 定时器仍活跃说明是提前结束（步骤推进 / 手动调用）
		if (DisplayDuration > 0.f && World->GetTimerManager().IsTimerActive(DisplayTimerHandle))
		{
			UE_LOG(LogTemp, Log, TEXT("[WidgetDisplay] Ended before display timer expired, removing widgets early"));
		}
		World->GetTimerManager().ClearTimer(DisplayTimerHandle);
	}

	int32 Removed = 0;
	for (TObjectPtr<UTaskWorldWidgetComponent>& CompPtr : CreatedWidgets)
	{
		if (UTaskWorldWidgetComponent* Comp = CompPtr.Get())
		{
			Comp->DestroyComponent();
			++Removed;
		}
	}
	CreatedWidgets.Empty();

	if (Removed > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[WidgetDisplay] Removed %d widget component(s)"), Removed);
	}

	Super::EndTask_Implementation();
}

// ==================== 受众与附着点解析 ====================

TArray<APawn*> UMultipleTask_WidgetDisplay::ResolveTargetPawns() const
{
	TArray<APawn*> Result;
	UTaskSequencerComponent* Seq = OwnerSequencer;
	if (!Seq)
	{
		return Result;
	}

	if (Audience == EPanelAudience::AllParticipants)
	{
		for (APlayerController* PC : Seq->GetParticipants())
		{
			// Pawn 未被占有（可能之后生成）或没带合法玩家标签（像素流客户端等）都跳过
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (Pawn && Seq->IsPawnValidPlayer(Pawn))
			{
				Result.AddUnique(Pawn);
			}
		}

		// 单机调试兜底：没有注册流程时用本地玩家（同样要求带合法玩家标签）
		if (Result.Num() == 0)
		{
			const UWorld* World = GetWorld();
			if (World && World->GetNetMode() == NM_Standalone)
			{
				if (APlayerController* PC = World->GetFirstPlayerController())
				{
					APawn* Pawn = PC->GetPawn();
					if (Pawn && Seq->IsPawnValidPlayer(Pawn))
					{
						Result.Add(Pawn);
					}
				}
			}
		}
	}
	else
	{
		// 指定身份：按身份 Tag 查 Pawn（RoleAssignments 优先，兜底按 Pawn Tag 匹配）
		for (const FName& RoleTag : TargetRoleTags)
		{
			if (APawn* Pawn = Seq->GetPawnByRoleTag(RoleTag))
			{
				Result.AddUnique(Pawn);
			}
		}
	}

	return Result;
}

USceneComponent* UMultipleTask_WidgetDisplay::FindAttachTarget(APawn* Pawn) const
{
	if (!Pawn)
	{
		return nullptr;
	}

	// 配置了 Tag 才按组件 Tag 查找（用户在 Pawn 上预放的锚点 SceneComponent）
	if (!AttachComponentTag.IsNone())
	{
		TArray<USceneComponent*> SceneComps;
		Pawn->GetComponents<USceneComponent>(SceneComps);
		for (USceneComponent* Comp : SceneComps)
		{
			if (Comp && Comp->ComponentHasTag(AttachComponentTag))
			{
				return Comp;
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("[WidgetDisplay] Pawn %s has no scene component tagged %s, fallback to root"),
			*Pawn->GetName(), *AttachComponentTag.ToString());
	}

	return Pawn->GetRootComponent();
}
