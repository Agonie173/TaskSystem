// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_EquipmentInteract.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

void UMultipleTask_EquipmentInteract::StartTask_Implementation()
{
	OriginalChoices.Empty();
	bConfirmReceived = false;

	// 查找装备柜（验证 Tag 正确）
	if (!EquipmentRackTag.IsNone())
	{
		AActor* Rack = FindActorByTag(EquipmentRackTag);
		if (!Rack)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EquipmentInteract] EquipmentRack with tag %s not found"),
				*EquipmentRackTag.ToString());
			// 不直接 EndTask，也许是执法记录仪等不需要装备柜的场景
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[EquipmentInteract] Task started, available equipments: %d"),
		AvailableEquipments.Num());
}

void UMultipleTask_EquipmentInteract::ReportEquipmentSelected(FName PlayerRoleTag, const FEquipmentInfo& SelectedEquipment)
{
	OriginalChoices.Add(PlayerRoleTag, SelectedEquipment);

	if (bRecordOriginalChoice)
	{
		UE_LOG(LogTemp, Log, TEXT("[EquipmentInteract] Player %s selected: %s"),
			*PlayerRoleTag.ToString(), *SelectedEquipment.DisplayName.ToString());
	}
}

void UMultipleTask_EquipmentInteract::ReportConfirmClicked(FName PlayerRoleTag)
{
	// 检查确认者身份是否匹配
	if (!ConfirmRoleTag.IsNone() && PlayerRoleTag != ConfirmRoleTag)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquipmentInteract] Player %s is not allowed to confirm (required: %s)"),
			*PlayerRoleTag.ToString(), *ConfirmRoleTag.ToString());
		return;
	}

	if (bConfirmReceived)
	{
		return;
	}

	bConfirmReceived = true;
	UE_LOG(LogTemp, Log, TEXT("[EquipmentInteract] Confirmation received from %s"), *PlayerRoleTag.ToString());

	if (bAutoAdjustAfterConfirm)
	{
		DoAutoAdjust();
	}

	EndTask();
}

void UMultipleTask_EquipmentInteract::DoAutoAdjust()
{
	UE_LOG(LogTemp, Log, TEXT("[EquipmentInteract] Auto-adjusting equipment to standard roles..."));

	for (const auto& Pair : StandardEquipmentByRoleTag)
	{
		const FName& RoleTag = Pair.Key;
		const FEquipmentInfo& StandardEquip = Pair.Value;

		APawn* Pawn = FindPawnByRoleTag(RoleTag);
		if (!Pawn)
		{
			continue;
		}

		// TODO: 这里需要 InventoryComponent 的配合
		// 伪代码：
		// 1. 收回 Pawn 当前持有的装备
		// 2. Spawn StandardEquip.EquipmentClass
		// 3. Attach 到右手 Socket

		UE_LOG(LogTemp, Log, TEXT("[EquipmentInteract] Role %s → Standard: %s"),
			*RoleTag.ToString(), *StandardEquip.DisplayName.ToString());
	}
}

void UMultipleTask_EquipmentInteract::CheckCompletion()
{
	if (bConfirmReceived)
	{
		EndTask();
	}
}
