// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_SpawnActor_OnClient.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "MultipleTask/TaskAudioBroadcaster.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

void UMultipleTask_SpawnActor_OnClient::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	UWorld* World = GetWorld();
	ATaskAudioBroadcaster* Broadcaster = OwnerSequencer ? OwnerSequencer->GetAudioBroadcaster() : nullptr;
	if (!ActorClass || !World || !Broadcaster)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnActor_OnClient] ActorClass or broadcaster invalid, task ends immediately"));
		EndTask();
		return;
	}

	// 服务器解算目标：合法玩家（VRPlayer/PCPlayer）+ 筛选标签（None = 不筛选）
	TArray<int32> TargetPlayerIds;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = Cast<APlayerController>(It->Get());
		if (!PC)
		{
			continue;
		}
		const APawn* Pawn = PC->GetPawn();
		if (!Pawn || !OwnerSequencer->IsPawnValidPlayer(Pawn))
		{
			continue;
		}
		if (!TargetFilterTag.IsNone() && !Pawn->ActorHasTag(TargetFilterTag))
		{
			continue;
		}
		if (const APlayerState* PS = PC->PlayerState)
		{
			TargetPlayerIds.Add(PS->GetPlayerId());
		}
	}

	if (TargetPlayerIds.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnActor_OnClient] No valid target player matched (filter tag: %s), task ends immediately"),
			TargetFilterTag.IsNone() ? TEXT("None") : *TargetFilterTag.ToString());
		EndTask();
		return;
	}

	// 广播到匹配玩家的客户端本地生成；句柄用于 EndTask 时广播删除
	LastSpawnHandle = Broadcaster->BroadcastSpawnActorOnClients(ActorClass, SpawnTransform, TargetPlayerIds, bDestroyOnStepEnd);
	UE_LOG(LogTemp, Log, TEXT("[SpawnActor_OnClient] Dispatched spawn to %d player(s) (handle=%d, destroy on step end: %d)"),
		TargetPlayerIds.Num(), LastSpawnHandle, bDestroyOnStepEnd);
}

void UMultipleTask_SpawnActor_OnClient::EndTask_Implementation()
{
	// 步骤推进/停止/重开走到这里：按配置广播删除各客户端的本地 Actor（基类保证只执行一次）
	if (LastSpawnHandle != INDEX_NONE)
	{
		if (bDestroyOnStepEnd && OwnerSequencer)
		{
			if (ATaskAudioBroadcaster* Broadcaster = OwnerSequencer->GetAudioBroadcaster())
			{
				Broadcaster->BroadcastDestroyLocalActor(LastSpawnHandle);
				UE_LOG(LogTemp, Log, TEXT("[SpawnActor_OnClient] Destroying local actors on clients (handle=%d)"), LastSpawnHandle);
			}
		}
		LastSpawnHandle = INDEX_NONE;
	}
	Super::EndTask_Implementation();
}
