// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_EquipmentConfirm.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	FString JoinTags(const TArray<FName>& Tags)
	{
		TArray<FString> Parts;
		Parts.Reserve(Tags.Num());
		for (const FName& Tag : Tags)
		{
			Parts.Add(Tag.ToString());
		}
		return FString::Join(Parts, TEXT("、"));
	}
}

UMultipleTask_EquipmentConfirm::UMultipleTask_EquipmentConfirm()
{
	// 面板默认类型与文案（可在派生 BP 里覆盖）
	PanelType = EUIPanelType::EquipmentCheck;
		ConfirmButtonText = FText::FromString(TEXT("确认装备并继续"));
}

// ==================== 生命周期 ====================

void UMultipleTask_EquipmentConfirm::StartTask_Implementation()
{
	ConfirmedRoles.Reset();

	// 基类下发面板（面板会展示全员当前装备）
	Super::StartTask_Implementation();

	if (StandardEquipment.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquipmentConfirm] No standard equipment configured, task ends immediately"));
		EndTask();
		return;
	}

	if (EquipmentTagPool.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquipmentConfirm] EquipmentTagPool is empty, all equipment will be treated as missing"));
	}

	UE_LOG(LogTemp, Log, TEXT("[EquipmentConfirm] Waiting for %d role(s) to confirm equipment"), StandardEquipment.Num());
}

void UMultipleTask_EquipmentConfirm::HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex)
{
	if (ButtonId != ConfirmButtonId || !PC)
	{
		return;
	}

	const FName RoleTag = OwnerSequencer ? OwnerSequencer->GetRoleTagByPawn(PC->GetPawn()) : NAME_None;
	if (RoleTag.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquipmentConfirm] %s confirmed without a role tag, ignored"), *PC->GetName());
		return;
	}

	if (IsRoleConfirmed(RoleTag))
	{
		return;
	}

	ConfirmedRoles.AddUnique(RoleTag);
	UE_LOG(LogTemp, Log, TEXT("[EquipmentConfirm] %s confirmed equipment: %s"),
		*RoleTag.ToString(), *JoinTags(GetSelectedEquipment(RoleTag)));

	// 锁定装备：给 Pawn 贴锁定标签，装备系统可检查此标签禁止再换装
	if (!EquipmentLockedTag.IsNone())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->Tags.AddUnique(EquipmentLockedTag);
			UE_LOG(LogTemp, Log, TEXT("[EquipmentConfirm] %s equipment locked (tag: %s)"),
				*RoleTag.ToString(), *EquipmentLockedTag.ToString());
		}
	}

	// 刷新全员面板，显示谁已确认
	PushPanelToAudience();

	// 调试日志：回执进度（在座身份中已确认 x/y）
	int32 PresentRoleCount = 0;
	int32 ConfirmedPresentCount = 0;
	for (const FRoleEquipmentTags& Standard : StandardEquipment)
	{
		if (!IsRolePresent(Standard.RoleTag))
		{
			continue;
		}
		++PresentRoleCount;
		if (IsRoleConfirmed(Standard.RoleTag))
		{
			++ConfirmedPresentCount;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[EquipmentConfirm] Confirm progress: %d/%d present role(s) confirmed"),
		ConfirmedPresentCount, PresentRoleCount);

	if (AreAllRolesConfirmed())
	{
		UE_LOG(LogTemp, Log, TEXT("[EquipmentConfirm] All present roles confirmed, ending task"));
		EndTask();
	}
}

// ==================== 核对 ====================

bool UMultipleTask_EquipmentConfirm::IsRolePresent(FName RoleTag) const
{
	return FindPawnByRoleTag(RoleTag) != nullptr;
}

bool UMultipleTask_EquipmentConfirm::AreAllRolesConfirmed() const
{
	// 只要求在座身份确认：缺席身份永远等不到点击，会把整个流程卡死
	int32 PresentRoleCount = 0;
	for (const FRoleEquipmentTags& Standard : StandardEquipment)
	{
		if (!IsRolePresent(Standard.RoleTag))
		{
			continue;
		}
		++PresentRoleCount;
		if (!IsRoleConfirmed(Standard.RoleTag))
		{
			return false;
		}
	}
	return PresentRoleCount > 0;
}

// ==================== 查询 ====================

TArray<FName> UMultipleTask_EquipmentConfirm::GetSelectedEquipment(FName RoleTag) const
{
	TArray<FName> Selected;

	const APawn* Pawn = FindPawnByRoleTag(RoleTag);
	if (!Pawn)
	{
		return Selected;
	}

	// 只统计 EquipmentTagPool 内的 Tag，Pawn 上的身份 Tag 等不会被当成装备
	for (const FName& Tag : Pawn->Tags)
	{
		if (EquipmentTagPool.Contains(Tag))
		{
			Selected.Add(Tag);
		}
	}
	return Selected;
}

bool UMultipleTask_EquipmentConfirm::IsRoleConfirmed(FName RoleTag) const
{
	return ConfirmedRoles.Contains(RoleTag);
}

// ==================== 面板 ====================

FTaskPanelData UMultipleTask_EquipmentConfirm::BuildPanelData() const
{
	FTaskPanelData Data = Super::BuildPanelData();

	for (const FRoleEquipmentTags& Standard : StandardEquipment)
	{
		const TArray<FName> Selected = GetSelectedEquipment(Standard.RoleTag);
		const bool bConfirmed = IsRoleConfirmed(Standard.RoleTag);

		const FString EquipText = Selected.Num() > 0 ? JoinTags(Selected) : FString(TEXT("未领取"));

		FPanelRowData Row;
		Row.Label = FText::FromName(Standard.RoleTag);
		Row.Value = FText::FromString(FString::Printf(TEXT("%s（%s）"), *EquipText,
			bConfirmed ? TEXT("已确认") : TEXT("未确认")));
		Row.ValueColor = bConfirmed ? FLinearColor::Green : FLinearColor::White;
		Data.Rows.Add(Row);
	}

	FTaskPanelButton Button;
	Button.ButtonId = ConfirmButtonId;
	Button.Label = ConfirmButtonText;
	Data.Buttons.Add(Button);

	return Data;
}
