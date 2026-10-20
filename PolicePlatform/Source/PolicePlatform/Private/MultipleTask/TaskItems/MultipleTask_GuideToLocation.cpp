// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_GuideToLocation.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
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
