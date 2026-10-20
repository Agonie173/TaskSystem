// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskSequencerComponent.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

UTaskSequencerComponent::UTaskSequencerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTaskSequencerComponent::BeginPlay()
{
	Super::BeginPlay();
	AssignRoleTagsToPlayers();
}

void UTaskSequencerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllTasks();
	Super::EndPlay(EndPlayReason);
}

// ==================== 公开接口 ====================

void UTaskSequencerComponent::LoadAndStartTask(UPrimaryTaskDataAsset* InTaskAsset)
{
	if (!InTaskAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TaskSequencer] LoadAndStartTask failed: TaskAsset is null"));
		return;
	}

	StopAllTasks();
	CurrentTaskAsset = InTaskAsset;
	CurrentStepIndex = -1;
	bIsRunning = true;

	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Loaded task: %s, total steps: %d"),
		*InTaskAsset->TaskName.ToString(), InTaskAsset->Steps.Num());

	// 启动第一步
	ExecuteStep(0);
}

void UTaskSequencerComponent::StopAllTasks()
{
	for (UMultipleTaskBase* Task : ActiveTasks)
	{
		if (Task)
		{
			Task->EndTask();
		}
	}
	ActiveTasks.Empty();
	CompletedTaskCount = 0;
	bIsRunning = false;
}

void UTaskSequencerComponent::SkipToStep(int32 StepIndex)
{
	if (!CurrentTaskAsset || !CurrentTaskAsset->Steps.IsValidIndex(StepIndex))
	{
		return;
	}

	StopAllTasks();
	bIsRunning = true;
	ExecuteStep(StepIndex);
}

int32 UTaskSequencerComponent::GetTotalStepCount() const
{
	return CurrentTaskAsset ? CurrentTaskAsset->Steps.Num() : 0;
}

// ==================== 事件转发 ====================

void UTaskSequencerComponent::ForwardOverlapBegin(AActor* OverlappedActor, AActor* OtherActor)
{
	for (UMultipleTaskBase* Task : ActiveTasks)
	{
		if (Task)
		{
			Task->OnActorOverlapBegin(OverlappedActor, OtherActor);
		}
	}
}

void UTaskSequencerComponent::ForwardOverlapEnd(AActor* OverlappedActor, AActor* OtherActor)
{
	for (UMultipleTaskBase* Task : ActiveTasks)
	{
		if (Task)
		{
			Task->OnActorOverlapEnd(OverlappedActor, OtherActor);
		}
	}
}

// ==================== 辅助查询 ====================

APawn* UTaskSequencerComponent::GetPawnByRoleTag(FName RoleTag) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 遍历所有 PlayerController，找 Pawn 上带 RoleTag 的
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn() && PC->GetPawn()->ActorHasTag(RoleTag))
		{
			return PC->GetPawn();
		}
	}
	return nullptr;
}

AActor* UTaskSequencerComponent::FindActorByTag(FName ActorTag) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 简单实现：遍历 World 中的 Actor
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->ActorHasTag(ActorTag))
		{
			return *It;
		}
	}
	return nullptr;
}

TArray<APawn*> UTaskSequencerComponent::GetAllPlayerPawns() const
{
	TArray<APawn*> Result;
	UWorld* World = GetWorld();
	if (!World)
	{
		return Result;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn())
		{
			Result.Add(PC->GetPawn());
		}
	}
	return Result;
}

// ==================== 内部实现 ====================

void UTaskSequencerComponent::ExecuteStep(int32 StepIndex)
{
	if (!CurrentTaskAsset || !CurrentTaskAsset->Steps.IsValidIndex(StepIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[TaskSequencer] ExecuteStep: invalid index %d"), StepIndex);
		return;
	}

	CurrentStepIndex = StepIndex;
	ActiveTasks.Empty();
	CompletedTaskCount = 0;

	const FTaskStep& Step = CurrentTaskAsset->Steps[StepIndex];
	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Step %d: %s (%d parallel tasks)"),
		StepIndex, *Step.StepTitle.ToString(), Step.ParallelTasks.Num());

	OnStepStarted.Broadcast(StepIndex);

	// 并行启动所有任务项
	// ParallelTasks 是 Instanced 属性，UHT 已经为每个 Step 创建了独立实例
	for (UMultipleTaskBase* TaskInstance : Step.ParallelTasks)
	{
		if (!TaskInstance)
		{
			CompletedTaskCount++;
			continue;
		}

		TaskInstance->OwnerSequencer = this;
		TaskInstance->OnTaskEnd.AddDynamic(this, &UTaskSequencerComponent::HandleTaskEnd);
		ActiveTasks.Add(TaskInstance);
		TaskInstance->StartTask();
	}

	// 如果当前 Step 没有任何任务（空数组或全空指针），直接完成
	CheckStepCompletion();
}

void UTaskSequencerComponent::CheckStepCompletion()
{
	if (!bIsRunning)
	{
		return;
	}

	if (CompletedTaskCount >= ActiveTasks.Num() && ActiveTasks.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Step %d completed!"), CurrentStepIndex);

		OnStepCompleted.Broadcast(CurrentStepIndex);

		// 下一步
		int32 NextStep = CurrentStepIndex + 1;
		if (CurrentTaskAsset && CurrentTaskAsset->Steps.IsValidIndex(NextStep))
		{
			ExecuteStep(NextStep);
		}
		else
		{
			// 所有步骤完成
			bIsRunning = false;
			ActiveTasks.Empty();
			OnAllStepsCompleted.Broadcast();
			UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] All steps completed!"));
		}
	}
}

void UTaskSequencerComponent::HandleTaskEnd()
{
	CompletedTaskCount++;
	CheckStepCompletion();
}

void UTaskSequencerComponent::AssignRoleTagsToPlayers()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	int32 RoleIndex = 1;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn())
		{
			FName RoleTag = FName(*FString::Printf(TEXT("Role_%d"), RoleIndex));
			PC->GetPawn()->Tags.AddUnique(RoleTag);
			UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Assigned tag %s to player pawn %s"),
				*RoleTag.ToString(), *PC->GetPawn()->GetName());
			RoleIndex++;
		}
	}
}
