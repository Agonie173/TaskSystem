// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_SpawnActor_OnServer.h"
#include "Engine/World.h"

void UMultipleTask_SpawnActor_OnServer::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	UWorld* World = GetWorld();
	if (!ActorClass || !World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnActor_OnServer] ActorClass invalid, task ends immediately"));
		EndTask();
		return;
	}

	// AlwaysSpawn：重叠碰撞不做调整，保证出现在配置的精确坐标
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnedActor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, Params);
	if (!SpawnedActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnActor_OnServer] SpawnActor failed for %s, task ends immediately"), *ActorClass->GetName());
		EndTask();
		return;
	}

	// 自动开启复制（实例级设置）：Actor 蓝图无需预开复制即可全员可见；
	// 同步移动使位置变化也能到达客户端
	if (!SpawnedActor->GetIsReplicated())
	{
		SpawnedActor->SetReplicates(true);
		SpawnedActor->SetReplicateMovement(true);
		UE_LOG(LogTemp, Log, TEXT("[SpawnActor_OnServer] Auto-enabled replication on %s"), *SpawnedActor->GetName());
	}

	UE_LOG(LogTemp, Log, TEXT("[SpawnActor_OnServer] Spawned %s at %s (destroy on step end: %d)"),
		*SpawnedActor->GetName(), *SpawnTransform.GetLocation().ToCompactString(), bDestroyOnStepEnd);
}

void UMultipleTask_SpawnActor_OnServer::EndTask_Implementation()
{
	// 步骤推进/停止/重开走到这里：按配置决定删留（基类 bTaskEnded 保证只执行一次）
	if (SpawnedActor)
	{
		if (bDestroyOnStepEnd)
		{
			UE_LOG(LogTemp, Log, TEXT("[SpawnActor_OnServer] Destroying %s on step end"), *SpawnedActor->GetName());
			SpawnedActor->Destroy();
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[SpawnActor_OnServer] Keeping %s after step end"), *SpawnedActor->GetName());
		}
		SpawnedActor = nullptr;
	}
	Super::EndTask_Implementation();
}
