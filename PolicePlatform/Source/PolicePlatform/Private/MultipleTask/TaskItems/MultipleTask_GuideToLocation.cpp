// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_GuideToLocation.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Components/BoxComponent.h"

void UMultipleTask_GuideToLocation::StartTask_Implementation()
{
	if (StationTag.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GuideToLocation] StationTag is not set"));
		EndTask();
		return;
	}

	if (RequiredRoleTag.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GuideToLocation] RequiredRoleTag is not set"));
		EndTask();
		return;
	}

	// 查找关卡中带 StationTag 的碰撞盒 Actor
	CachedStationActor = FindActorByTag(StationTag);
	if (!CachedStationActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GuideToLocation] Station Actor with tag %s not found"), *StationTag.ToString());
		EndTask();
		return;
	}

	bRoleInStation = false;

	// 生成引导标志
	if (bShowMarker && !MarkerClass.IsNull())
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FVector SpawnLoc = CachedStationActor->GetActorLocation() + FVector(0, 0, MarkerHeightOffset);
			FRotator SpawnRot = CachedStationActor->GetActorRotation();

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnedMarker = World->SpawnActor<AActor>(MarkerClass.LoadSynchronous(), SpawnLoc, SpawnRot, Params);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[GuideToLocation] Waiting for %s to enter station %s"),
		*RequiredRoleTag.ToString(), *StationTag.ToString());

	// 任务开始时玩家可能已经站在站位里（重入/延迟启动），按当前重叠补一次判定
	TArray<AActor*> OverlappingActors;
	CachedStationActor->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
	for (AActor* Actor : OverlappingActors)
	{
		if (Actor && Actor->ActorHasTag(RequiredRoleTag))
		{
			bRoleInStation = true;
			UE_LOG(LogTemp, Log, TEXT("[GuideToLocation] %s already in station %s at task start"),
				*Actor->GetName(), *StationTag.ToString());
			CheckAndComplete();
			break;
		}
	}
}

void UMultipleTask_GuideToLocation::EndTask_Implementation()
{
	// Skipped/中断结束也要清掉引导标志，避免残留在关卡里
	if (SpawnedMarker)
	{
		SpawnedMarker->Destroy();
		SpawnedMarker = nullptr;
	}

	Super::EndTask_Implementation();
}

void UMultipleTask_GuideToLocation::OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor)
{
	// 我们只关心目标站位碰撞盒触发的事件
	if (OverlappedActor != CachedStationActor)
	{
		return;
	}

	if (!OtherActor)
	{
		return;
	}

	// 检查进入者是否是我们要等的身份
	if (OtherActor->ActorHasTag(RequiredRoleTag))
	{
		bRoleInStation = true;
		UE_LOG(LogTemp, Log, TEXT("[GuideToLocation] %s entered station %s"),
			*OtherActor->GetName(), *StationTag.ToString());
		CheckAndComplete();
	}
}

void UMultipleTask_GuideToLocation::OnActorOverlapEnd(AActor* OverlappedActor, AActor* OtherActor)
{
	if (OverlappedActor != CachedStationActor)
	{
		return;
	}

	if (!OtherActor)
	{
		return;
	}

	if (OtherActor->ActorHasTag(RequiredRoleTag))
	{
		// 离开站位，取消到位标志
		bRoleInStation = false;
		UE_LOG(LogTemp, Log, TEXT("[GuideToLocation] %s left station %s, cancelled"),
			*OtherActor->GetName(), *StationTag.ToString());
	}
}

void UMultipleTask_GuideToLocation::CheckAndComplete()
{
	if (bRoleInStation)
	{
		UE_LOG(LogTemp, Log, TEXT("[GuideToLocation] Station %s complete for %s"),
			*StationTag.ToString(), *RequiredRoleTag.ToString());

		// 清理引导标志
		if (SpawnedMarker)
		{
			SpawnedMarker->Destroy();
			SpawnedMarker = nullptr;
		}

		EndTask();
	}
}
