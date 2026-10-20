// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_EquipmentInteract.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
	const APawn* Pawn = FindPawnByRoleTag(Requirement.RoleTag);
	if (!Pawn)
	{
		return false;
	}

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
}
