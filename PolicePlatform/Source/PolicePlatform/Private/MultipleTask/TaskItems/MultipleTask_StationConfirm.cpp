// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_StationConfirm.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Actor.h"

UMultipleTask_StationConfirm::UMultipleTask_StationConfirm()
{
	PanelType = EUIPanelType::ScenePrompt;
	PanelTitle = FText::FromString(TEXT("就位确认"));
	PanelContent = FText::FromString(TEXT("三名玩家全部回到警戒位置后，由任意一名玩家确认。"));
	ConfirmButtonText = FText::FromString(TEXT("确认警戒"));
	NotReadyReason = FText::FromString(TEXT("三名玩家未全部到位"));
}

// ==================== 生命周期 ====================

void UMultipleTask_StationConfirm::StartTask_Implementation()
{
	ArrivedFlags.Empty();
	CachedStations.Reset();

	if (Requirements.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StationConfirm] No requirements configured, task ends immediately"));
		EndTask();
		return;
	}

	for (const FRoleStationPair& Requirement : Requirements)
	{
		AActor* Station = FindActorByTag(Requirement.StationTag);
		if (!Station)
		{
			UE_LOG(LogTemp, Warning, TEXT("[StationConfirm] Station tag %s not found"),
				*Requirement.StationTag.ToString());
			EndTask();
			return;
		}
		CachedStations.Add(Station);
	}

	// 按当前重叠初始化（玩家可能已经站好了）
	InitArrivalFromOverlaps();

	// 基类下发面板（BuildPanelData 会按到位情况启用/禁用确认按钮）
	Super::StartTask_Implementation();

	UE_LOG(LogTemp, Log, TEXT("[StationConfirm] Waiting for %d role(s) to be in position"), Requirements.Num());
}

void UMultipleTask_StationConfirm::InitArrivalFromOverlaps()
{
	ArrivedFlags.Init(false, Requirements.Num());

	for (int32 Index = 0; Index < Requirements.Num(); ++Index)
	{
		APawn* RequiredPawn = FindPawnByRoleTag(Requirements[Index].RoleTag);
		AActor* Station = CachedStations.IsValidIndex(Index) ? CachedStations[Index] : nullptr;
		ArrivedFlags[Index] = IsPawnInStation(RequiredPawn, Station);
	}
}

bool UMultipleTask_StationConfirm::IsPawnInStation(APawn* Pawn, AActor* Station) const
{
	if (!Pawn || !Station)
	{
		return false;
	}

	TArray<AActor*> OverlappingPawns;
	Station->GetOverlappingActors(OverlappingPawns, APawn::StaticClass());
	return OverlappingPawns.Contains(Pawn);
}

// ==================== 事件 ====================

void UMultipleTask_StationConfirm::OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!OverlappedActor || !OtherActor)
	{
		return;
	}

	bool bChanged = false;

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
			bChanged = true;
			UE_LOG(LogTemp, Log, TEXT("[StationConfirm] %s arrived at station %s"),
				*Requirements[Index].RoleTag.ToString(), *Requirements[Index].StationTag.ToString());
		}
	}

	if (bChanged)
	{
		PushPanelToAudience();
	}
}

void UMultipleTask_StationConfirm::OnActorOverlapEnd(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!OverlappedActor || !OtherActor)
	{
		return;
	}

	bool bChanged = false;

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
			const bool bStillInStation = IsPawnInStation(RequiredPawn, CachedStations[Index]);
			if (bStillInStation)
			{
				continue;
			}

			ArrivedFlags[Index] = false;
			bChanged = true;
			UE_LOG(LogTemp, Log, TEXT("[StationConfirm] %s left station %s"),
				*Requirements[Index].RoleTag.ToString(), *Requirements[Index].StationTag.ToString());
		}
	}

	if (bChanged)
	{
		PushPanelToAudience();
	}
}

// ==================== 确认 ====================

bool UMultipleTask_StationConfirm::AreAllArrived() const
{
	if (ArrivedFlags.Num() != Requirements.Num() || Requirements.Num() == 0)
	{
		return false;
	}

	for (const bool bArrived : ArrivedFlags)
	{
		if (!bArrived)
		{
			return false;
		}
	}
	return true;
}

void UMultipleTask_StationConfirm::HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex)
{
	if (ButtonId != ConfirmButtonId)
	{
		return;
	}

	if (!AreAllArrived())
	{
		UE_LOG(LogTemp, Warning, TEXT("[StationConfirm] %s confirmed before all roles arrived"), PC ? *PC->GetName() : TEXT("Unknown"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[StationConfirm] Confirmed by %s, task ending"), PC ? *PC->GetName() : TEXT("Unknown"));
	EndTask();
}

// ==================== 面板 ====================

FTaskPanelData UMultipleTask_StationConfirm::BuildPanelData() const
{
	FTaskPanelData Data = Super::BuildPanelData();

	const bool bAllArrived = AreAllArrived();

	for (int32 Index = 0; Index < Requirements.Num(); ++Index)
	{
		const bool bArrived = ArrivedFlags.IsValidIndex(Index) && ArrivedFlags[Index];

		FPanelRowData Row;
		Row.Label = FText::FromName(Requirements[Index].RoleTag);
		Row.Value = FText::FromString(bArrived ? TEXT("已到位") : TEXT("未到位"));
		Row.ValueColor = bArrived ? FLinearColor::Green : FLinearColor::Red;
		Data.Rows.Add(Row);
	}

	FTaskPanelButton Button;
	Button.ButtonId = ConfirmButtonId;
	Button.Label = ConfirmButtonText;
	Button.bEnabled = bAllArrived;
	Button.DisabledReason = NotReadyReason;
	Data.Buttons.Add(Button);

	return Data;
}
