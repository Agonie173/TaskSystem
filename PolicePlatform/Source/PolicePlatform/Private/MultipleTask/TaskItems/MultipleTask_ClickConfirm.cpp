// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_ClickConfirm.h"

#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "MultipleTask/TaskAudioBroadcaster.h"
#include "MultipleTask/Actors/WidgetConfirmActor.h"
#include "MultipleTask/UI/TaskPanelChannelComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

const FName UMultipleTask_ClickConfirm::WidgetConfirmEventId(TEXT("WidgetConfirm"));

void UMultipleTask_ClickConfirm::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	// 服务器权威：没有 Sequencer 说明没注入，无法筛玩家
	if (!OwnerSequencer)
	{
		UE_LOG(LogTemp, Error, TEXT("[ClickConfirm] StartTask: OwnerSequencer not injected, cannot resolve target players"));
		EndTask();
		return;
	}

	if (TargetPlayerTags.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClickConfirm] StartTask: TargetPlayerTags is empty, no players need to confirm, ending immediately"));
		EndTask();
		return;
	}

	if (!WidgetActorClass.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[ClickConfirm] StartTask: WidgetActorClass not configured"));
		EndTask();
		return;
	}

	// ========== 1. 按 TargetPlayerTags 筛目标 PC ==========
	TargetPCs.Empty();
	ConfirmedPCs.Empty();

	for (APlayerController* PC : OwnerSequencer->GetParticipants())
	{
		if (!PC) continue;
		if (APawn* Pawn = PC->GetPawn())
		{
			for (const FName& Tag : TargetPlayerTags)
			{
				if (Pawn->ActorHasTag(Tag))
				{
					TargetPCs.Add(PC);
					break;
				}
			}
		}
	}

	if (TargetPCs.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClickConfirm] StartTask: No target players matched tags (Count=%d)"),
			TargetPlayerTags.Num());
		EndTask();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ClickConfirm] StartTask: Resolved %d target players (tags count: %d, Mode: %s)"),
		TargetPCs.Num(), TargetPlayerTags.Num(),
		bRequireAllPlayers ? TEXT("All") : TEXT("Any"));

	// ========== 2. 取 Broadcaster ==========
	ATaskAudioBroadcaster* Broadcaster = OwnerSequencer->GetAudioBroadcaster();
	if (!Broadcaster)
	{
		UE_LOG(LogTemp, Error, TEXT("[ClickConfirm] StartTask: No TaskAudioBroadcaster available"));
		EndTask();
		return;
	}

	// ========== 3. 把 TaskInstanceId 注入 WidgetActorClass 的 CDO ==========
	// 客户端 Spawn 时从默认对象读 TaskInstanceId，ReportConfirmed 上报时原样带回。
	// 注：并行多个 ClickConfirm 会互相覆盖 CDO 的 TaskInstanceId——这种场景极少，暂不处理。
	if (UClass* ActorClass = WidgetActorClass.Get())
	{
		AActor* CDO = ActorClass->GetDefaultObject<AActor>();
		if (AWidgetConfirmActor* CDOConfirm = Cast<AWidgetConfirmActor>(CDO))
		{
			CDOConfirm->TaskInstanceId = GetFName();
		}
	}

	// 收集 TargetPlayerIds（PlayerState.GetPlayerId() 为凭据；Actor Tags 不复制，客户端不能用标签自判）
	TArray<int32> TargetPlayerIds;
	for (const TWeakObjectPtr<APlayerController>& WeakPC : TargetPCs)
	{
		APlayerController* PC = WeakPC.Get();
		if (PC && PC->PlayerState)
		{
			TargetPlayerIds.Add(PC->PlayerState->GetPlayerId());
		}
	}

	if (TargetPlayerIds.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClickConfirm] StartTask: No PlayerIds resolved from target PCs"));
		EndTask();
		return;
	}

	// ========== 4. 广播生成 ==========
	// WidgetSpawnTransform 为世界绝对坐标；客户端生成时 bDestroyOnStepEnd = true 表示 EndTask 自动清理
	SpawnHandle = Broadcaster->BroadcastSpawnActorOnClients(
		WidgetActorClass.Get(),
		WidgetSpawnTransform,
		TargetPlayerIds,
		/*bDestroyOnStepEnd=*/bDestroyOnEnd);

	if (SpawnHandle == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("[ClickConfirm] StartTask: Broadcaster returned invalid handle"));
		EndTask();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ClickConfirm] StartTask: Broadcast spawn dispatched (handle=%d, TaskInstanceId=%s)"),
		SpawnHandle, *GetFName().ToString());
}

void UMultipleTask_ClickConfirm::EndTask_Implementation()
{
	if (bTaskEnded) return;
	Super::EndTask_Implementation();

	if (SpawnHandle != INDEX_NONE && OwnerSequencer)
	{
		if (ATaskAudioBroadcaster* Broadcaster = OwnerSequencer->GetAudioBroadcaster())
		{
			Broadcaster->BroadcastDestroyLocalActor(SpawnHandle);
			UE_LOG(LogTemp, Log, TEXT("[ClickConfirm] EndTask: Broadcast destroy (handle=%d)"), SpawnHandle);
		}
		SpawnHandle = INDEX_NONE;
	}
}

void UMultipleTask_ClickConfirm::OnCustomEvent(APlayerController* PC, FName EventId, const FString& Payload)
{
	// 只处理 WidgetConfirm 事件
	if (EventId != WidgetConfirmEventId) return;

	// Payload = TaskInstanceId.ToString()，过滤是否发给自己
	if (!IsPayloadSelf(Payload))
	{
		// 不是发给我（并行多个 ClickConfirm 任务时，事件会转发给所有活跃任务）
		return;
	}

	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClickConfirm] OnCustomEvent: PC is null"));
		return;
	}

	if (!IsPCTarget(PC))
	{
		UE_LOG(LogTemp, Log, TEXT("[ClickConfirm] OnCustomEvent: PC %s not in target list, ignored"),
			*PC->GetName());
		return;
	}

	if (IsPCConfirmed(PC))
	{
		UE_LOG(LogTemp, Log, TEXT("[ClickConfirm] OnCustomEvent: PC %s already confirmed, ignored"),
			*PC->GetName());
		return;
	}

	// 登记确认
	ConfirmedPCs.Add(PC);
	UE_LOG(LogTemp, Log, TEXT("[ClickConfirm] OnCustomEvent: PC %s confirmed (%d/%d)"),
		*PC->GetName(), ConfirmedPCs.Num(), TargetPCs.Num());

	CheckCompletion();
}

bool UMultipleTask_ClickConfirm::IsPayloadSelf(const FString& Payload) const
{
	return Payload == GetFName().ToString();
}

bool UMultipleTask_ClickConfirm::IsPCConfirmed(APlayerController* PC) const
{
	for (const TWeakObjectPtr<APlayerController>& Weak : ConfirmedPCs)
	{
		if (Weak.Get() == PC) return true;
	}
	return false;
}

bool UMultipleTask_ClickConfirm::IsPCTarget(APlayerController* PC) const
{
	for (const TWeakObjectPtr<APlayerController>& Weak : TargetPCs)
	{
		if (Weak.Get() == PC) return true;
	}
	return false;
}

void UMultipleTask_ClickConfirm::CheckCompletion()
{
	if (bTaskEnded) return;

	bool bDone = false;
	if (bRequireAllPlayers)
	{
		bDone = (ConfirmedPCs.Num() >= TargetPCs.Num());
	}
	else
	{
		bDone = (ConfirmedPCs.Num() >= 1);
	}

	if (bDone)
	{
		UE_LOG(LogTemp, Log, TEXT("[ClickConfirm] CheckCompletion: satisfied (%d/%d), ending task"),
			ConfirmedPCs.Num(), TargetPCs.Num());
		EndTask();
	}
}
