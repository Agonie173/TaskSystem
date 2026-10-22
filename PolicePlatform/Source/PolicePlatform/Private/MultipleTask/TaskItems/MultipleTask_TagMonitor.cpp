// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_TagMonitor.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMultipleTask_TagMonitor::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	if (MonitoredRoles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TagMonitor] No roles configured, ending immediately: %s"), *TaskName.ToString());
		EndTask();
		return;
	}

	// 初始检查：如果全部已经满足则立即结束
	if (AreAllRolesSatisfied())
	{
		UE_LOG(LogTemp, Log, TEXT("[TagMonitor] All roles already satisfied at start: %s"), *TaskName.ToString());
		EndTask();
		return;
	}

	// 启动低频轮询
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CheckTimerHandle, this, &UMultipleTask_TagMonitor::CheckRequirements, CheckInterval, true);
		UE_LOG(LogTemp, Log, TEXT("[TagMonitor] Started monitoring %d role(s), interval=%.1fs: %s"),
			MonitoredRoles.Num(), CheckInterval, *TaskName.ToString());
	}
}

void UMultipleTask_TagMonitor::EndTask_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimerHandle);
	}
	Super::EndTask_Implementation();
}

bool UMultipleTask_TagMonitor::AreAllRolesSatisfied() const
{
	if (MonitoredRoles.Num() == 0)
	{
		return true;
	}

	for (const auto& Pair : MonitoredRoles)
	{
		if (!IsRoleSatisfied(Pair.Key))
		{
			return false;
		}
	}
	return true;
}

bool UMultipleTask_TagMonitor::IsRoleSatisfied(FName RoleTag) const
{
	const FTagList* RequiredTags = MonitoredRoles.Find(RoleTag);
	if (!RequiredTags || RequiredTags->Tags.Num() == 0)
	{
		return true;
	}

	APawn* Pawn = FindPawnByRoleTag(RoleTag);
	return IsPawnTagged(Pawn, RequiredTags->Tags);
}

TArray<FName> UMultipleTask_TagMonitor::GetUnsatisfiedRoles() const
{
	TArray<FName> Result;
	for (const auto& Pair : MonitoredRoles)
	{
		if (!IsRoleSatisfied(Pair.Key))
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

void UMultipleTask_TagMonitor::CheckRequirements()
{
	if (bTaskEnded)
	{
		return;
	}

	if (bRequireAllRoles)
	{
		if (AreAllRolesSatisfied())
		{
			UE_LOG(LogTemp, Log, TEXT("[TagMonitor] All roles satisfied, ending: %s"), *TaskName.ToString());
			EndTask();
			return;
		}
	}
	else
	{
		// 任意一个满足即结束
		for (const auto& Pair : MonitoredRoles)
		{
			if (IsRoleSatisfied(Pair.Key))
			{
				UE_LOG(LogTemp, Log, TEXT("[TagMonitor] Role %s satisfied (any-role mode), ending: %s"),
					*Pair.Key.ToString(), *TaskName.ToString());
				EndTask();
				return;
			}
		}
	}

	// 输出进度日志
	TArray<FName> Unsatisfied = GetUnsatisfiedRoles();
	UE_LOG(LogTemp, Verbose, TEXT("[TagMonitor] Waiting for %d role(s): %s"),
		Unsatisfied.Num(), *FString::JoinBy(Unsatisfied, TEXT(", "), [](const FName& N) { return N.ToString(); }));
}

bool UMultipleTask_TagMonitor::IsPawnTagged(APawn* Pawn, const TArray<FName>& RequiredTags) const
{
	if (!Pawn)
	{
		return false;
	}

	for (const FName& Tag : RequiredTags)
	{
		if (!Tag.IsNone() && Pawn->ActorHasTag(Tag))
		{
			return true;
		}
	}
	return false;
}
