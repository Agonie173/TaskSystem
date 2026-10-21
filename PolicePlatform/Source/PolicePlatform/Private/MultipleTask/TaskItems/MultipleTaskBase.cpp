// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "MultipleTask/Score/ScoreComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

void UMultipleTaskBase::StartTask_Implementation()
{
	bTaskEnded = false;

	UE_LOG(LogTemp, Log, TEXT("[MultipleTaskBase] Task start: Name=%s"), *TaskName.ToString());
}

void UMultipleTaskBase::EndTask_Implementation()
{
	// 同一个任务只允许广播一次，否则 Sequencer 的完成计数会翻倍
	if (bTaskEnded)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[MultipleTaskBase] EndTask ignored: task already ended, Name=%s"), *TaskName.ToString());
		return;
	}
	bTaskEnded = true;

	UE_LOG(LogTemp, Log, TEXT("[MultipleTaskBase] Task end, broadcasting OnTaskEnd: Name=%s"), *TaskName.ToString());

	OnTaskEnd.Broadcast(this);
}

void UMultipleTaskBase::OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor)
{
}

void UMultipleTaskBase::OnActorOverlapEnd(AActor* OverlappedActor, AActor* OtherActor)
{
}

APawn* UMultipleTaskBase::FindPawnByRoleTag(FName RoleTag) const
{
	if (OwnerSequencer)
	{
		return OwnerSequencer->GetPawnByRoleTag(RoleTag);
	}

	UE_LOG(LogTemp, Warning, TEXT("[MultipleTaskBase] FindPawnByRoleTag failed: OwnerSequencer is null, RoleTag=%s"), *RoleTag.ToString());
	return nullptr;
}

AActor* UMultipleTaskBase::FindActorByTag(FName ActorTag) const
{
	if (OwnerSequencer)
	{
		return OwnerSequencer->FindActorByTag(ActorTag);
	}

	UE_LOG(LogTemp, Warning, TEXT("[MultipleTaskBase] FindActorByTag failed: OwnerSequencer is null, ActorTag=%s"), *ActorTag.ToString());
	return nullptr;
}

void UMultipleTaskBase::ReportScoreEvent(FName RoleTag, FName ErrorCode)
{
	if (RoleTag.IsNone() || ErrorCode.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MultipleTask] ReportScoreEvent ignored: RoleTag or ErrorCode is None"));
		return;
	}

	UScoreComponent* ScoreComp = UScoreComponent::FindOrCreateScoreComponent(GetWorld());
	if (!ScoreComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MultipleTask] ReportScoreEvent: no score component (client side or no GameState)"));
		return;
	}

	ScoreComp->ReportScoreEvent(RoleTag, ErrorCode);

	UE_LOG(LogTemp, Log, TEXT("[MultipleTaskBase] Score event reported: Role=%s, ErrorCode=%s"), *RoleTag.ToString(), *ErrorCode.ToString());
}
