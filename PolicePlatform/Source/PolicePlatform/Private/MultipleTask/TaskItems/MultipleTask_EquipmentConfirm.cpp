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
	PanelType = EUIPanelType::EquipmentCheck;
	PanelTitle = FText::FromString(TEXT("自主选择装备"));
	PanelContent = FText::FromString(TEXT("请根据研判和岗位分工领取装备，确认后将按标准装备继续。"));
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

	// 刷新全员面板，显示谁已确认
	PushPanelToAudience();

	if (AreAllRolesConfirmed())
	{
		EvaluateAndFinish();
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

void UMultipleTask_EquipmentConfirm::EvaluateAndFinish()
{
	for (const FRoleEquipmentTags& Standard : StandardEquipment)
	{
		// 缺席身份不做“少拿/多拿”判定，避免给不存在的角色记分
		if (!IsRolePresent(Standard.RoleTag))
		{
			continue;
		}

		const TArray<FName> Missing = GetMissingEquipment(Standard.RoleTag);
		const TArray<FName> Wrong = GetWrongEquipment(Standard.RoleTag);

		UE_LOG(LogTemp, Log, TEXT("[EquipmentConfirm] %s selected [%s], missing [%s], wrong [%s]"),
			*Standard.RoleTag.ToString(),
			*JoinTags(GetSelectedEquipment(Standard.RoleTag)),
			*JoinTags(Missing),
			*JoinTags(Wrong));

		if (Missing.Num() > 0)
		{
			ReportScoreEvent(Standard.RoleTag, MissingEquipmentErrorCode);
		}
		if (Wrong.Num() > 0)
		{
			ReportScoreEvent(Standard.RoleTag, WrongEquipmentErrorCode);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[EquipmentConfirm] All roles confirmed, task ending"));
	EndTask();
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

	for (const FName& Tag : Pawn->Tags)
	{
		if (EquipmentTagPool.Contains(Tag))
		{
			Selected.Add(Tag);
		}
	}
	return Selected;
}

TArray<FName> UMultipleTask_EquipmentConfirm::GetMissingEquipment(FName RoleTag) const
{
	TArray<FName> Missing;

	const TArray<FName> Selected = GetSelectedEquipment(RoleTag);
	for (const FRoleEquipmentTags& Standard : StandardEquipment)
	{
		if (Standard.RoleTag != RoleTag)
		{
			continue;
		}

		for (const FName& Tag : Standard.RequiredTags)
		{
			if (!Tag.IsNone() && !Selected.Contains(Tag))
			{
				Missing.Add(Tag);
			}
		}
	}
	return Missing;
}

TArray<FName> UMultipleTask_EquipmentConfirm::GetWrongEquipment(FName RoleTag) const
{
	TArray<FName> Wrong;

	const TArray<FName> Selected = GetSelectedEquipment(RoleTag);
	for (const FRoleEquipmentTags& Standard : StandardEquipment)
	{
		if (Standard.RoleTag != RoleTag)
		{
			continue;
		}

		for (const FName& Tag : Selected)
		{
			if (!Standard.RequiredTags.Contains(Tag))
			{
				Wrong.Add(Tag);
			}
		}
		break;
	}
	return Wrong;
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
