// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskSequencerComponent.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask/UI/TaskPanelChannelComponent.h"
#include "MultipleTask/Score/ScoreComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UTaskSequencerComponent::UTaskSequencerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTaskSequencerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllTasks();
	Super::EndPlay(EndPlayReason);
}

void UTaskSequencerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UTaskSequencerComponent, ReplicatedOccupiedRoles);
	DOREPLIFETIME(UTaskSequencerComponent, CurrentStepIndex);
	DOREPLIFETIME(UTaskSequencerComponent, bIsRunning);
}

bool UTaskSequencerComponent::HasAuthorityForTask() const
{
	return GetOwnerRole() == ROLE_Authority;
}

// ==================== 任务流程 ====================

void UTaskSequencerComponent::LoadAndStartTask(UPrimaryTaskDataAsset* InTaskAsset)
{
	if (!HasAuthorityForTask())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TaskSequencer] LoadAndStartTask ignored: no authority"));
		return;
	}

	if (!InTaskAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TaskSequencer] LoadAndStartTask failed: TaskAsset is null"));
		return;
	}

	StopAllTasks();
	CurrentTaskAsset = InTaskAsset;
	CurrentStepIndex = -1;
	bIsRunning = true;

	// 新一轮训练重新选身份，避免沿用上一局的身份直接跳过 04 步
	ClearAllRoles();

	// 新一轮训练从零开始计分
	if (UScoreComponent* ScoreComp = UScoreComponent::FindScoreComponent(GetWorld()))
	{
		ScoreComp->ResetScores();
	}

	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Loaded task: %s, total steps: %d"),
		*InTaskAsset->TaskName.ToString(), InTaskAsset->Steps.Num());

	ExecuteStep(0);
}

void UTaskSequencerComponent::StopAllTasks()
{
	// 必须先停止运行标记，否则 EndTask 广播会触发 CheckStepCompletion，
	// 在遍历过程中推进到下一步（会清空/重建 ActiveTasks 导致迭代器失效）
	bIsRunning = false;
	bStepStarting = false;
	CompletedTaskCount = 0;

	TArray<TObjectPtr<UMultipleTaskBase>> TasksToStop = MoveTemp(ActiveTasks);
	ActiveTasks.Empty();

	for (UMultipleTaskBase* Task : TasksToStop)
	{
		if (!Task)
		{
			continue;
		}
		Task->OnTaskEnd.RemoveDynamic(this, &UTaskSequencerComponent::HandleTaskEnd);
		Task->EndTask();
	}
}

void UTaskSequencerComponent::SkipToStep(int32 StepIndex)
{
	if (!HasAuthorityForTask())
	{
		return;
	}

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

int32 UTaskSequencerComponent::FinishActiveTasksByClass(TSubclassOf<UMultipleTaskBase> TaskClass)
{
	if (!HasAuthorityForTask() || !TaskClass)
	{
		return 0;
	}

	int32 FinishedCount = 0;
	// 任务 EndTask 会触发 CheckStepCompletion，用快照避免遍历中容器变化
	TArray<TObjectPtr<UMultipleTaskBase>> Snapshot = ActiveTasks;

	for (UMultipleTaskBase* Task : Snapshot)
	{
		if (Task && !Task->bTaskEnded && Task->IsA(TaskClass))
		{
			Task->EndTask();
			FinishedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Finished %d active task(s) of class %s"),
		FinishedCount, *TaskClass->GetName());
	return FinishedCount;
}

void UTaskSequencerComponent::NotifyChoiceMade(FName ChoiceId, APlayerController* PC)
{
	if (!HasAuthorityForTask() || ChoiceId.IsNone())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Choice %s made by %s"),
		*ChoiceId.ToString(), PC ? *PC->GetName() : TEXT("Unknown"));
	OnTaskChoiceMade.Broadcast(ChoiceId, PC);
}

// ==================== 参训玩家注册 ====================

bool UTaskSequencerComponent::RegisterParticipant(APlayerController* PC)
{
	if (!HasAuthorityForTask() || !PC)
	{
		return false;
	}

	for (const TWeakObjectPtr<APlayerController>& Existing : Participants)
	{
		if (Existing.Get() == PC)
		{
			return true;
		}
	}

	Participants.Add(PC);
	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Participant registered: %s (total %d)"),
		*PC->GetName(), Participants.Num());

	// 提前创建面板通道：保证客户端 UI 在第一个面板到达前就能绑定事件
	UTaskPanelChannelComponent::FindOrCreateChannel(PC);
	return true;
}

void UTaskSequencerComponent::UnregisterParticipant(APlayerController* PC)
{
	if (!HasAuthorityForTask() || !PC)
	{
		return;
	}

	Participants.RemoveAll([PC](const TWeakObjectPtr<APlayerController>& Item)
	{
		return !Item.IsValid() || Item.Get() == PC;
	});

	// 同时释放该玩家占用的身份
	for (auto It = RoleAssignments.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid() || It.Value().Get() == PC)
		{
			It.RemoveCurrent();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Participant unregistered: %s"), *PC->GetName());

	RefreshOccupiedRoles();
}

bool UTaskSequencerComponent::AssignRole(FName RoleTag, APlayerController* PC)
{
	if (!HasAuthorityForTask() || RoleTag.IsNone() || !PC)
	{
		return false;
	}

	// 角色已被其他玩家占用则拒绝
	if (const TWeakObjectPtr<APlayerController>* Existing = RoleAssignments.Find(RoleTag))
	{
		if (Existing->IsValid() && Existing->Get() != PC)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TaskSequencer] Role %s already taken by %s"),
				*RoleTag.ToString(), *Existing->Get()->GetName());
			return false;
		}
	}

	// 清掉该玩家原有身份，避免一人占两个角色
	for (auto It = RoleAssignments.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid() || It.Value().Get() == PC)
		{
			if (APawn* OldPawn = PC->GetPawn())
			{
				OldPawn->Tags.Remove(It.Key());
			}
			It.RemoveCurrent();
		}
	}

	RegisterParticipant(PC);
	RoleAssignments.Add(RoleTag, PC);

	// 同步 Pawn 上的身份 Tag（供依赖 ActorHasTag 的判定使用；真相源仍是 RoleAssignments）
	if (APawn* Pawn = PC->GetPawn())
	{
		Pawn->Tags.AddUnique(RoleTag);
	}

	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Role %s assigned to %s"),
		*RoleTag.ToString(), *PC->GetName());

	RefreshOccupiedRoles();
	return true;
}

bool UTaskSequencerComponent::IsParticipant(APlayerController* PC) const
{
	if (!PC)
	{
		return false;
	}

	for (const TWeakObjectPtr<APlayerController>& Existing : Participants)
	{
		if (Existing.Get() == PC)
		{
			return true;
		}
	}
	return false;
}

TArray<APlayerController*> UTaskSequencerComponent::GetParticipants() const
{
	TArray<APlayerController*> Result;
	for (const TWeakObjectPtr<APlayerController>& Item : Participants)
	{
		if (APlayerController* PC = Item.Get())
		{
			Result.Add(PC);
		}
	}
	return Result;
}

APlayerController* UTaskSequencerComponent::GetControllerByRoleTag(FName RoleTag) const
{
	if (RoleTag.IsNone())
	{
		return nullptr;
	}

	if (const TWeakObjectPtr<APlayerController>* Found = RoleAssignments.Find(RoleTag))
	{
		if (APlayerController* PC = Found->Get())
		{
			return PC;
		}
	}

	// 兜底：还没走身份分配流程时，按 Pawn 上的 Tag 找
	for (const TWeakObjectPtr<APlayerController>& Item : Participants)
	{
		APlayerController* PC = Item.Get();
		if (PC && PC->GetPawn() && PC->GetPawn()->ActorHasTag(RoleTag))
		{
			return PC;
		}
	}
	return nullptr;
}

bool UTaskSequencerComponent::IsRoleAssigned(FName RoleTag) const
{
	// 服务器本地查询保留直接映射，客户端则读复制列表
	if (HasAuthorityForTask())
	{
		const TWeakObjectPtr<APlayerController>* Found = RoleAssignments.Find(RoleTag);
		return Found && Found->IsValid();
	}
	return ReplicatedOccupiedRoles.Contains(RoleTag);
}

TArray<FName> UTaskSequencerComponent::GetOccupiedRoles() const
{
	return ReplicatedOccupiedRoles;
}

void UTaskSequencerComponent::ClearAllRoles()
{
	if (!HasAuthorityForTask())
	{
		return;
	}

	for (auto It = RoleAssignments.CreateIterator(); It; ++It)
	{
		if (It.Value().IsValid())
		{
			if (APawn* Pawn = It.Value()->GetPawn())
			{
				Pawn->Tags.Remove(It.Key());
			}
		}
		It.RemoveCurrent();
	}

	RefreshOccupiedRoles();
}

void UTaskSequencerComponent::RefreshOccupiedRoles()
{
	if (!HasAuthorityForTask())
	{
		return;
	}

	TArray<FName> NewRoles;
	for (const auto& Pair : RoleAssignments)
	{
		if (Pair.Value.IsValid())
		{
			NewRoles.Add(Pair.Key);
		}
	}
	NewRoles.Sort(FNameLexicalLess());
	ReplicatedOccupiedRoles = MoveTemp(NewRoles);

	if (const UWorld* World = GetWorld())
	{
		if (World->GetNetMode() != NM_DedicatedServer)
		{
			OnRep_OccupiedRoles();
		}
	}
}

void UTaskSequencerComponent::OnRep_OccupiedRoles()
{
	OnRoleOccupancyChanged.Broadcast();
}

void UTaskSequencerComponent::OnRep_CurrentStepIndex()
{
	// 服务器直接广播过 OnStepStarted，这里只负责客户端侧的 HUD 刷新
	if (CurrentStepIndex >= 0)
	{
		OnStepStarted.Broadcast(CurrentStepIndex);
	}
}

// ==================== 事件转发 ====================

void UTaskSequencerComponent::ForwardOverlapBegin(AActor* OverlappedActor, AActor* OtherActor)
{
	// 复制一份快照：任务在 EndTask 时可能改写 ActiveTasks
	TArray<TObjectPtr<UMultipleTaskBase>> Snapshot = ActiveTasks;
	for (UMultipleTaskBase* Task : Snapshot)
	{
		if (Task && !Task->bTaskEnded)
		{
			Task->OnActorOverlapBegin(OverlappedActor, OtherActor);
		}
	}
}

void UTaskSequencerComponent::ForwardOverlapEnd(AActor* OverlappedActor, AActor* OtherActor)
{
	TArray<TObjectPtr<UMultipleTaskBase>> Snapshot = ActiveTasks;
	for (UMultipleTaskBase* Task : Snapshot)
	{
		if (Task && !Task->bTaskEnded)
		{
			Task->OnActorOverlapEnd(OverlappedActor, OtherActor);
		}
	}
}

// ==================== 辅助查询 ====================

APawn* UTaskSequencerComponent::GetPawnByRoleTag(FName RoleTag) const
{
	if (APlayerController* PC = GetControllerByRoleTag(RoleTag))
	{
		return PC->GetPawn();
	}
	return nullptr;
}

AActor* UTaskSequencerComponent::FindActorByTag(FName ActorTag) const
{
	UWorld* World = GetWorld();
	if (!World || ActorTag.IsNone())
	{
		return nullptr;
	}

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
	for (const TWeakObjectPtr<APlayerController>& Item : Participants)
	{
		if (APlayerController* PC = Item.Get())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				Result.Add(Pawn);
			}
		}
	}
	return Result;
}

FName UTaskSequencerComponent::GetRoleTagByPawn(APawn* Pawn) const
{
	if (!Pawn)
	{
		return NAME_None;
	}

	for (const auto& Pair : RoleAssignments)
	{
		if (Pair.Value.IsValid() && Pair.Value.Get()->GetPawn() == Pawn)
		{
			return Pair.Key;
		}
	}
	return NAME_None;
}

// ==================== 评分 ====================

bool UTaskSequencerComponent::FindScoreRule(FName ErrorCode, FScoreRule& OutRule) const
{
	if (!CurrentTaskAsset || ErrorCode.IsNone())
	{
		return false;
	}

	auto FindInStep = [ErrorCode, &OutRule](const FTaskStep& Step)
	{
		for (const FScoreRule& Rule : Step.ScoreRules)
		{
			if (Rule.ErrorCode == ErrorCode)
			{
				OutRule = Rule;
				return true;
			}
		}
		return false;
	};

	if (CurrentTaskAsset->Steps.IsValidIndex(CurrentStepIndex)
		&& FindInStep(CurrentTaskAsset->Steps[CurrentStepIndex]))
	{
		return true;
	}

	for (const FTaskStep& Step : CurrentTaskAsset->Steps)
	{
		if (FindInStep(Step))
		{
			return true;
		}
	}
	return false;
}

// ==================== 内部实现 ====================

void UTaskSequencerComponent::ExecuteStep(int32 StepIndex)
{
	if (!CurrentTaskAsset || !CurrentTaskAsset->Steps.IsValidIndex(StepIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[TaskSequencer] ExecuteStep: invalid index %d"), StepIndex);
		return;
	}

	// 清理上一步的运行时实例
	for (UMultipleTaskBase* OldTask : ActiveTasks)
	{
		if (OldTask)
		{
			OldTask->OnTaskEnd.RemoveDynamic(this, &UTaskSequencerComponent::HandleTaskEnd);
		}
	}
	ActiveTasks.Empty();
	CompletedTaskCount = 0;
	CurrentStepIndex = StepIndex;

	const FTaskStep& Step = CurrentTaskAsset->Steps[StepIndex];
	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Step %d: %s (%d parallel tasks)"),
		StepIndex, *Step.StepTitle.ToString(), Step.ParallelTasks.Num());

	// 先进入“启动中”状态再广播：防止监听者在回调里 Stop/Skip 与后续实例化交错
	bStepStarting = true;
	OnStepStarted.Broadcast(StepIndex);

	// 第一阶段：全部实例化并挂回调，此阶段不启动任何任务
	for (UMultipleTaskBase* SourceTask : Step.ParallelTasks)
	{
		if (!SourceTask)
		{
			CompletedTaskCount++;
			continue;
		}

		// 配表里的对象归属 DataAsset，是共享模板；运行时必须实例化一份，
		// 否则多次训练会共用内部状态、并重复累积委托绑定
		UMultipleTaskBase* TaskInstance = DuplicateObject<UMultipleTaskBase>(SourceTask, this);
		if (!TaskInstance)
		{
			CompletedTaskCount++;
			continue;
		}

		TaskInstance->OnTaskEnd.Clear();
		TaskInstance->OwnerSequencer = this;
		TaskInstance->bTaskEnded = false;
		TaskInstance->OnTaskEnd.AddDynamic(this, &UTaskSequencerComponent::HandleTaskEnd);

		ActiveTasks.Add(TaskInstance);
	}

	// 第二阶段：统一启动（任务可能在 StartTask 内立刻结束，此时计数是完整的）
	for (UMultipleTaskBase* Task : ActiveTasks)
	{
		if (Task)
		{
			Task->StartTask();
		}
	}
	bStepStarting = false;

	CheckStepCompletion();
}

void UTaskSequencerComponent::CheckStepCompletion()
{
	if (!bIsRunning || bStepStarting)
	{
		return;
	}

	if (CompletedTaskCount < ActiveTasks.Num())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Step %d completed!"), CurrentStepIndex);
	OnStepCompleted.Broadcast(CurrentStepIndex);

	const int32 NextStep = CurrentStepIndex + 1;
	if (CurrentTaskAsset && CurrentTaskAsset->Steps.IsValidIndex(NextStep))
	{
		ExecuteStep(NextStep);
	}
	else
	{
		bIsRunning = false;

		for (UMultipleTaskBase* Task : ActiveTasks)
		{
			if (Task)
			{
				Task->OnTaskEnd.RemoveDynamic(this, &UTaskSequencerComponent::HandleTaskEnd);
			}
		}
		ActiveTasks.Empty();

		OnAllStepsCompleted.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] All steps completed!"));
	}
}

void UTaskSequencerComponent::HandleTaskEnd(UMultipleTaskBase* EndedTask)
{
	if (!bIsRunning || !EndedTask)
	{
		return;
	}

	// 忽略已不属于当前步骤的过期回调
	if (!ActiveTasks.Contains(EndedTask))
	{
		return;
	}

	CompletedTaskCount++;
	CheckStepCompletion();
}
