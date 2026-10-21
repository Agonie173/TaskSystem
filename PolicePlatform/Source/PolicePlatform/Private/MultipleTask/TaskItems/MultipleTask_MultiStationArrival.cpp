// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_MultiStationArrival.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

void UMultipleTask_MultiStationArrival::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	ArrivedFlags.Empty();
	CachedStations.Reset();

	if (Requirements.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MultiStationArrival] No requirements configured, task ends immediately"));
		EndTask();
		return;
	}

	ArrivedFlags.Init(false, Requirements.Num());
	for (const FRoleStationPair& Requirement : Requirements)
	{
		AActor* Station = FindActorByTag(Requirement.StationTag);
		if (!Station)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MultiStationArrival] Station tag %s not found"),
				*Requirement.StationTag.ToString());
			EndTask();
			return;
		}
		CachedStations.Add(Station);
	}

	// 玩家可能已经站在里面了（任务切换时），先按当前重叠初始化
	InitArrivalFromOverlaps();

	UE_LOG(LogTemp, Log, TEXT("[MultiStationArrival] Waiting for %d role(s) to stand simultaneously"),
		Requirements.Num());

	CheckAndComplete();
}

void UMultipleTask_MultiStationArrival::InitArrivalFromOverlaps()
{
	for (int32 Index = 0; Index < Requirements.Num(); ++Index)
	{
		APawn* RequiredPawn = FindPawnByRoleTag(Requirements[Index].RoleTag);
		AActor* Station = CachedStations.IsValidIndex(Index) ? CachedStations[Index] : nullptr;
		ArrivedFlags[Index] = IsPawnInStation(RequiredPawn, Station);
	}
}

bool UMultipleTask_MultiStationArrival::IsPawnInStation(APawn* Pawn, AActor* Station) const
{
	if (!Pawn || !Station)
	{
		return false;
	}

	TArray<AActor*> OverlappingPawns;
	Station->GetOverlappingActors(OverlappingPawns, APawn::StaticClass());
	return OverlappingPawns.Contains(Pawn);
}

void UMultipleTask_MultiStationArrival::OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!OverlappedActor || !OtherActor)
	{
		return;
	}

	for (int32 Index = 0; Index < Requirements.Num(); ++Index)
	{
		if (!CachedStations.IsValidIndex(Index) || CachedStations[Index] != OverlappedActor)
		{
			continue;
		}

		APawn* RequiredPawn = FindPawnByRoleTag(Requirements[Index].RoleTag);
		if (RequiredPawn && RequiredPawn == OtherActor && !ArrivedFlags[Index])
		{
			ArrivedFlags[Index] = true;
			UE_LOG(LogTemp, Log, TEXT("[MultiStationArrival] %s entered station %s"),
				*Requirements[Index].RoleTag.ToString(), *Requirements[Index].StationTag.ToString());
			CheckAndComplete();
		}
	}
}

void UMultipleTask_MultiStationArrival::OnActorOverlapEnd(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!OverlappedActor || !OtherActor)
	{
		return;
	}

	for (int32 Index = 0; Index < Requirements.Num(); ++Index)
	{
		if (!CachedStations.IsValidIndex(Index) || CachedStations[Index] != OverlappedActor)
		{
			continue;
		}

		APawn* RequiredPawn = FindPawnByRoleTag(Requirements[Index].RoleTag);
		if (RequiredPawn && RequiredPawn == OtherActor && ArrivedFlags[Index])
		{
			ArrivedFlags[Index] = false;
			UE_LOG(LogTemp, Log, TEXT("[MultiStationArrival] %s left station %s, cancelled"),
				*Requirements[Index].RoleTag.ToString(), *Requirements[Index].StationTag.ToString());
		}
	}
}

void UMultipleTask_MultiStationArrival::CheckAndComplete()
{
	if (ArrivedFlags.Num() != Requirements.Num())
	{
		return;
	}

	for (const bool bArrived : ArrivedFlags)
	{
		if (!bArrived)
		{
			return;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[MultiStationArrival] All roles arrived simultaneously, task ending"));
	EndTask();
}
