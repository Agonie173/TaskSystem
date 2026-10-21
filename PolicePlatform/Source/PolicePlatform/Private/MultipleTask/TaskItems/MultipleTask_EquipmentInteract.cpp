// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_EquipmentInteract.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	// 仅用于调试日志：把 Tag 数组拼成逗号分隔字符串
	FString JoinTagsForLog(const TArray<FName>& Tags)
	{
		TArray<FString> Parts;
		Parts.Reserve(Tags.Num());
		for (const FName& Tag : Tags)
		{
			Parts.Add(Tag.ToString());
		}
		return FString::Join(Parts, TEXT(","));
	}
}

void UMultipleTask_EquipmentInteract::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	if (Requirements.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquipmentInteract] No equipment requirements configured, task ends immediately"));
		EndTask();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[EquipmentInteract] Task started, %d role requirement(s)"), Requirements.Num());

	// 可能一开始就已满足（重入/调试）
	if (AreAllRequirementsMet())
	{
		UE_LOG(LogTemp, Log, TEXT("[EquipmentInteract] All requirements already met"));
		EndTask();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		// 启动低频轮询：装备 Tag 变化没有事件通知，只能定时比对
		World->GetTimerManager().SetTimer(CheckTimerHandle, this,
			&UMultipleTask_EquipmentInteract::CheckRequirements, CheckInterval, true);
	}
}

void UMultipleTask_EquipmentInteract::EndTask_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimerHandle);
	}
	Super::EndTask_Implementation();
}

bool UMultipleTask_EquipmentInteract::AreAllRequirementsMet() const
{
	if (Requirements.Num() == 0)
	{
		return false;
	}

	for (const FRoleEquipmentTags& Requirement : Requirements)
	{
		if (!IsRoleSatisfied(Requirement))
		{
			return false;
		}
	}
	return true;
}

TArray<FName> UMultipleTask_EquipmentInteract::GetMissingTags(FName RoleTag) const
{
	TArray<FName> Missing;

	// 该身份的 Pawn 不在场时，所有必需 Tag 都视为缺失
	const APawn* Pawn = FindPawnByRoleTag(RoleTag);
	for (const FRoleEquipmentTags& Requirement : Requirements)
	{
		if (Requirement.RoleTag != RoleTag)
		{
			continue;
		}

		for (const FName& Tag : Requirement.RequiredTags)
		{
			if (!Tag.IsNone() && (!Pawn || !Pawn->ActorHasTag(Tag)))
			{
				Missing.Add(Tag);
			}
		}
	}
	return Missing;
}

bool UMultipleTask_EquipmentInteract::IsRoleSatisfied(const FRoleEquipmentTags& Requirement) const
{
	// 找不到该身份的 Pawn（未分配/已离场）视为不满足，任务会持续等待
	const APawn* Pawn = FindPawnByRoleTag(Requirement.RoleTag);
	if (!Pawn)
	{
		return false;
	}

	// 逐一核对必需 Tag：少拿任何一个都算未达标（多拿不做检测）
	for (const FName& Tag : Requirement.RequiredTags)
	{
		if (!Tag.IsNone() && !Pawn->ActorHasTag(Tag))
		{
			return false;
		}
	}
	return true;
}

void UMultipleTask_EquipmentInteract::CheckRequirements()
{
	if (AreAllRequirementsMet())
	{
		UE_LOG(LogTemp, Log, TEXT("[EquipmentInteract] All requirements met, task ending"));
		EndTask();
	}
	else
	{
		// 调试日志：轮询进度快照（已达标身份 x/y + 未达标身份的缺口明细）
		int32 SatisfiedCount = 0;
		for (const FRoleEquipmentTags& Requirement : Requirements)
		{
			if (IsRoleSatisfied(Requirement))
			{
				++SatisfiedCount;
			}
			else
			{
				UE_LOG(LogTemp, Verbose, TEXT("[EquipmentInteract] Role %s still missing tags: [%s]"),
					*Requirement.RoleTag.ToString(), *JoinTagsForLog(GetMissingTags(Requirement.RoleTag)));
			}
		}
		UE_LOG(LogTemp, Verbose, TEXT("[EquipmentInteract] Progress: %d/%d role(s) satisfied"),
			SatisfiedCount, Requirements.Num());
	}
}
