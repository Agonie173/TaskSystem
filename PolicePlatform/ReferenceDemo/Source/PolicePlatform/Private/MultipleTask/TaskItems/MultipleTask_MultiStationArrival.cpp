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

		// 身份既没分配、当前也没有任何 Pawn 带这个 Tag 时提前告警；
		// Pawn 可能之后才生成/打 Tag，到站判定在 Overlap 时仍会重查
		if (!FindPawnByRoleTag(Requirement.RoleTag))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[MultiStationArrival] RoleTag %s: not assigned and no current pawn carries this tag; will re-check on overlap"),
				*Requirement.RoleTag.ToString());
		}
	}

	// 玩家可能已经站在里面了（任务切换时），先按当前重叠初始化
	InitArrivalFromOverlaps();

	UE_LOG(LogTemp, Log, TEXT("[MultiStationArrival] Waiting for %d role(s) to stand simultaneously"),
		Requirements.Num());

	CheckAndComplete();
}

void UMultipleTask_MultiStationArrival::InitArrivalFromOverlaps()
{
	// 逐条按"当前应到 Pawn 是否已在站位重叠列表里"初始化，不依赖 Overlap 事件补发
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

	// 以站位 Actor 的实际重叠列表为准（汇总其所有碰撞组件的结果），
	// 而不是单次 OverlapEnd 事件，避免多碰撞组件导致的"假离开"误判
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

	UE_LOG(LogTemp, Verbose, TEXT("[MultiStationArrival] Overlap begin on %s by %s"),
		*OverlappedActor->GetName(), *OtherActor->GetName());

	// 只处理配表缓存的站位触发的事件，其他 Actor 的重叠一律忽略
	for (int32 Index = 0; Index < Requirements.Num(); ++Index)
	{
		if (!CachedStations.IsValidIndex(Index) || CachedStations[Index] != OverlappedActor)
		{
			continue;
		}

		// 判定口径：先按 RoleTag 解析出"应到 Pawn"，且它必须正是本次进站的 OtherActor，
		// 其他 Pawn（无身份/别的身份）进站不影响本 Requirement
		APawn* RequiredPawn = FindPawnByRoleTag(Requirements[Index].RoleTag);
		if (RequiredPawn && RequiredPawn == OtherActor && !ArrivedFlags[Index])
		{
			ArrivedFlags[Index] = true;
			UE_LOG(LogTemp, Log, TEXT("[MultiStationArrival] %s entered station %s"),
				*Requirements[Index].RoleTag.ToString(), *Requirements[Index].StationTag.ToString());

			// 进度回执：统计当前已到位的 Requirement 数量（只读，不影响判定）
			int32 ArrivedCount = 0;
			for (const bool bArrived : ArrivedFlags)
			{
				if (bArrived)
				{
					++ArrivedCount;
				}
			}
			UE_LOG(LogTemp, Log, TEXT("[MultiStationArrival] Progress %d/%d arrived"),
				ArrivedCount, Requirements.Num());

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

	UE_LOG(LogTemp, Verbose, TEXT("[MultiStationArrival] Overlap end on %s by %s"),
		*OverlappedActor->GetName(), *OtherActor->GetName());

	for (int32 Index = 0; Index < Requirements.Num(); ++Index)
	{
		if (!CachedStations.IsValidIndex(Index) || CachedStations[Index] != OverlappedActor)
		{
			continue;
		}

		APawn* RequiredPawn = FindPawnByRoleTag(Requirements[Index].RoleTag);
		if (RequiredPawn && RequiredPawn == OtherActor && ArrivedFlags[Index])
		{
			// 站位点可能有多个碰撞组件：离开其中一个不代表真的离开，用实际重叠复核
			if (IsPawnInStation(RequiredPawn, CachedStations[Index]))
			{
				continue;
			}

			ArrivedFlags[Index] = false;
			UE_LOG(LogTemp, Log, TEXT("[MultiStationArrival] %s left station %s, cancelled"),
				*Requirements[Index].RoleTag.ToString(), *Requirements[Index].StationTag.ToString());
		}
	}
}

void UMultipleTask_MultiStationArrival::CheckAndComplete()
{
	// ArrivedFlags 尚未按 Requirements 初始化齐时不做判定，避免以残缺状态误判完成
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
