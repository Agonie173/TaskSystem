// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "MultipleTask/Score/ScoreComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

void UMultipleTaskBase::StartTask_Implementation()
{
	bTaskEnded = false;
}

void UMultipleTaskBase::EndTask_Implementation()
{
	// 同一个任务只允许广播一次，否则 Sequencer 的完成计数会翻倍
	if (bTaskEnded)
	{
		return;
	}
	bTaskEnded = true;

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
	return nullptr;
}

AActor* UMultipleTaskBase::FindActorByTag(FName ActorTag) const
{
	if (OwnerSequencer)
	{
		return OwnerSequencer->FindActorByTag(ActorTag);
	}
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
}
