// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "GameFramework/Pawn.h"

void UMultipleTaskBase::StartTask_Implementation()
{
}

void UMultipleTaskBase::EndTask_Implementation()
{
	OnTaskEnd.Broadcast();
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
