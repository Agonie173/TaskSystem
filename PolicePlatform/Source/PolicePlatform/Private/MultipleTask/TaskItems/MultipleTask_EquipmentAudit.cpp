// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_EquipmentAudit.h"
#include "GameFramework/Pawn.h"

void UMultipleTask_EquipmentAudit::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	if (StandardEquipment.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquipmentAudit] No standard equipment configured, ending immediately"));
		EndTask();
		return;
	}

	if (EquipmentTagPool.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquipmentAudit] EquipmentTagPool is empty, all equipment will be treated as missing"));
	}

	EvaluateAndReport();

	UE_LOG(LogTemp, Log, TEXT("[EquipmentAudit] Audit completed, ending task"));
	EndTask();
}

// ==================== 核对 ====================

void UMultipleTask_EquipmentAudit::EvaluateAndReport()
{
	for (const FRoleEquipmentTags& Standard : StandardEquipment)
	{
		// 缺席身份不做"少拿/多拿"判定，避免给不存在的角色记分
		if (!IsRolePresent(Standard.RoleTag))
		{
			continue;
		}

		const TArray<FName> Selected = GetSelectedEquipment(Standard.RoleTag);
		const TArray<FName> Missing = GetMissingEquipment(Standard.RoleTag);
		const TArray<FName> Wrong = GetWrongEquipment(Standard.RoleTag);

		UE_LOG(LogTemp, Log, TEXT("[EquipmentAudit] %s selected [%s], missing [%s], wrong [%s]"),
			*Standard.RoleTag.ToString(),
			*FString::JoinBy(Selected, TEXT("、"), [](const FName& N) { return N.ToString(); }),
			*FString::JoinBy(Missing, TEXT("、"), [](const FName& N) { return N.ToString(); }),
			*FString::JoinBy(Wrong, TEXT("、"), [](const FName& N) { return N.ToString(); }));

		if (Missing.Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[EquipmentAudit] Report missing-equipment error '%s' for role %s"),
				*MissingEquipmentErrorCode.ToString(), *Standard.RoleTag.ToString());
			ReportScoreEvent(Standard.RoleTag, MissingEquipmentErrorCode);
		}
		if (Wrong.Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[EquipmentAudit] Report wrong-equipment error '%s' for role %s"),
				*WrongEquipmentErrorCode.ToString(), *Standard.RoleTag.ToString());
			ReportScoreEvent(Standard.RoleTag, WrongEquipmentErrorCode);
		}
	}
}

// ==================== 查询 ====================

TArray<FName> UMultipleTask_EquipmentAudit::GetSelectedEquipment(FName RoleTag) const
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

TArray<FName> UMultipleTask_EquipmentAudit::GetMissingEquipment(FName RoleTag) const
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

TArray<FName> UMultipleTask_EquipmentAudit::GetWrongEquipment(FName RoleTag) const
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

bool UMultipleTask_EquipmentAudit::IsRolePresent(FName RoleTag) const
{
	return FindPawnByRoleTag(RoleTag) != nullptr;
}
