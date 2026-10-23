// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskSequencerComponent.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask/UI/TaskPanelChannelComponent.h"
#include "MultipleTask/TaskAudioBroadcaster.h"
#include "MultipleTask/Score/ScoreComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UTaskSequencerComponent::UTaskSequencerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTaskSequencerComponent::BeginPlay()
{
	Super::BeginPlay();

	// 立即生成音频广播通道（仅服务器侧生效，客户端调用返回空）：
	// 动态生成的复制 Actor 必须先于玩家进场存在，客户端连接时通道才随连接初始化建立；
	// 否则首次播放时的 Multicast RPC 会因客户端尚未收到该 Actor 而被静默丢弃（Reliable 也不补发）
	GetAudioBroadcaster();
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

	// 玩家不足（DS 启动即加载 / LS 下远程客户端尚未连接的典型情形）：挂起等待。
	// 否则受众解析（面板/提示/Widget/音频）全部落空，客户端也收不到任何 Multicast——
	// 复制通道未建立时 RPC 会被静默丢弃（Reliable 也不会补发）。
	// LS 的盲区：host 玩家启动瞬间就存在，Participants/GetNumPlayers 都非零，
	// 必须按配表的 MinPlayersToStart（含 host 在内的期望总人数）等待
	UWorld* World = GetWorld();
	const int32 MinPlayers = FMath::Max(1, InTaskAsset->MinPlayersToStart);
	const int32 CurrentPlayers = World && World->GetAuthGameMode() ? World->GetAuthGameMode()->GetNumPlayers() : 0;
	if (World && !World->IsNetMode(NM_Standalone) && CurrentPlayers < MinPlayers)
	{
		PendingTaskAsset = InTaskAsset;
		UE_LOG(LogTemp, Warning, TEXT("[TaskSequencer] Players %d/%d, task load deferred until enough players join"),
			CurrentPlayers, MinPlayers);
		World->GetTimerManager().SetTimer(WaitForPlayerTimerHandle, this,
			&UTaskSequencerComponent::RetryPendingTaskLoad, 0.5f, true);
		return;
	}

	StopAllTasks();
	CurrentTaskAsset = InTaskAsset;
	CurrentStepIndex = -1;
	bIsRunning = true;

	// 新一轮训练重新选身份，避免沿用上一局的身份直接跳过 04 步
	ClearAllRoles();

	// 简化接入兜底：外层框架若没有注册流程，则把当前所有 PlayerController 视为参训玩家。
	// 否则多人局 Participants 为空：面板受众为 0（谁都不下发）、
	// WaitForConfirm/EquipmentConfirm 的“全员确认”永远无法达成。
	if (Participants.Num() == 0)
	{
		int32 AutoRegistered = 0;
		int32 SkippedByTag = 0;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				// Pawn 已存在但没带合法玩家标签 → 视为像素流客户端等非玩家，跳过；
				// Pawn 还没被占有 → 照旧注册（Pawn 可能之后才生成，任务内会兜底查找）
				const APawn* Pawn = PC->GetPawn();
				if (Pawn && !IsPawnValidPlayer(Pawn))
				{
					++SkippedByTag;
					UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Auto register skipped (pawn has no valid player tag): %s"), *PC->GetName());
					continue;
				}

				if (RegisterParticipant(PC))
				{
					++AutoRegistered;
				}
			}
		}

		// 全部被标签过滤掉时大概率是 Pawn 上忘了打 VRPlayer/PCPlayer 标签，给出醒目提示
		if (AutoRegistered == 0 && SkippedByTag > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TaskSequencer] All %d pawn(s) skipped by ValidPlayerTags filter - check that pawns carry one of the valid player tags"),
				SkippedByTag);
		}

		UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] No participant registered yet, auto-registered %d player controller(s)"),
			AutoRegistered);
	}

	// 新一轮训练从零开始计分
	if (UScoreComponent* ScoreComp = UScoreComponent::FindScoreComponent(GetWorld()))
	{
		ScoreComp->ResetScores();
	}

	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Loaded task: %s, total steps: %d"),
		*InTaskAsset->TaskName.ToString(), InTaskAsset->Steps.Num());

	ExecuteStep(0);
}

void UTaskSequencerComponent::RetryPendingTaskLoad()
{
	// 挂起配表已被清（StopAllTasks）时停掉轮询
	UPrimaryTaskDataAsset* Asset = PendingTaskAsset.Get();
	UWorld* World = GetWorld();
	if (!Asset || !World)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(WaitForPlayerTimerHandle);
		}
		PendingTaskAsset = nullptr;
		return;
	}

	// 玩家数（GameMode 统计，含 host）未达配表要求则继续等
	if (!World->GetAuthGameMode() || World->GetAuthGameMode()->GetNumPlayers() < FMath::Max(1, Asset->MinPlayersToStart))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(WaitForPlayerTimerHandle);
	PendingTaskAsset = nullptr;
	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Enough players joined (%d), starting deferred task"), World->GetAuthGameMode()->GetNumPlayers());

	LoadAndStartTask(Asset);
}

void UTaskSequencerComponent::StopAllTasks()
{
	// 取消"等待首个玩家"的挂起加载（若有）
	if (PendingTaskAsset)
	{
		PendingTaskAsset = nullptr;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(WaitForPlayerTimerHandle);
		}
		UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Cancelled deferred task load"));
	}

	// 取消步骤延迟启动定时器（若有）
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepStartDelayTimerHandle);
	}

	// 必须先停止运行标记，否则 EndTask 广播会触发 CheckStepCompletion，
	// 在遍历过程中推进到下一步（会清空/重建 ActiveTasks 导致迭代器失效）
	bIsRunning = false;
	bStepStarting = false;

	TArray<TObjectPtr<UMultipleTaskBase>> TasksToStop = MoveTemp(ActiveTasks);
	ActiveTasks.Empty();

	if (TasksToStop.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Stopping %d active task(s)"), TasksToStop.Num());
	}

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
		UE_LOG(LogTemp, Warning, TEXT("[TaskSequencer] SkipToStep failed: invalid index %d (total %d)"),
			StepIndex, GetTotalStepCount());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Skip to step %d"), StepIndex);
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

bool UTaskSequencerComponent::IsPawnValidPlayer(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}

	// 列表为空 = 未配置过滤，放行所有 Pawn（保持旧行为）
	if (ValidPlayerTags.Num() == 0)
	{
		return true;
	}

	for (const FName& Tag : ValidPlayerTags)
	{
		if (!Tag.IsNone() && Pawn->ActorHasTag(Tag))
		{
			return true;
		}
	}
	return false;
}

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
	if (UTaskPanelChannelComponent* Channel = UTaskPanelChannelComponent::FindOrCreateChannel(PC))
	{
		// 绑定自定义事件转发：客户端语音识别等结果经通道上报后路由到当前活跃任务
		Channel->OnCustomEventSubmitted.RemoveDynamic(this, &UTaskSequencerComponent::ForwardCustomEvent);
		Channel->OnCustomEventSubmitted.AddDynamic(this, &UTaskSequencerComponent::ForwardCustomEvent);
	}

	// 广播玩家注册事件，供监听型任务（PlayerListener 等）感知新玩家加入
	if (APawn* Pawn = PC->GetPawn())
	{
		OnPlayerRegistered.Broadcast(PC, Pawn);
	}
	else
	{
		// Pawn 尚未占有也广播（参数为空），监听方需自行等待 Pawn 可用
		OnPlayerRegistered.Broadcast(PC, nullptr);
	}

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
	else
	{
		// 占有链还没建立：依赖 Pawn Tag 兜底的任务（站位/装备等）会临时找不到该身份
		UE_LOG(LogTemp, Warning, TEXT("[TaskSequencer] Role %s assigned to %s but pawn not possessed yet; pawn tag not synced"),
			*RoleTag.ToString(), *PC->GetName());
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

	// 兜底：还没走身份分配流程时，按 Pawn 上的 Tag 找。
	// 这里必须遍历世界中所有 PlayerController，而不能只查 Participants：
	// 测试关卡/简化接入往往没调用 RegisterParticipant，只给 Pawn 打了身份 Tag。
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<APlayerController> It(World); It; ++It)
		{
			APlayerController* PC = *It;
			if (PC && PC->GetPawn() && PC->GetPawn()->ActorHasTag(RoleTag))
			{
				return PC;
			}
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

	const int32 ClearedCount = RoleAssignments.Num();
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

	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Cleared %d role assignment(s)"), ClearedCount);
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
	UE_LOG(LogTemp, Verbose, TEXT("[TaskSequencer] Client OnRep_CurrentStepIndex: %d"), CurrentStepIndex);
	if (CurrentStepIndex >= 0)
	{
		OnStepStarted.Broadcast(CurrentStepIndex);
	}
}

// ==================== 事件转发 ====================

void UTaskSequencerComponent::ForwardOverlapBegin(AActor* OverlappedActor, AActor* OtherActor)
{
	UE_LOG(LogTemp, Verbose, TEXT("[TaskSequencer] OverlapBegin forwarded: trigger=%s other=%s"),
		*GetNameSafe(OverlappedActor), *GetNameSafe(OtherActor));

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
	UE_LOG(LogTemp, Verbose, TEXT("[TaskSequencer] OverlapEnd forwarded: trigger=%s other=%s"),
		*GetNameSafe(OverlappedActor), *GetNameSafe(OtherActor));

	TArray<TObjectPtr<UMultipleTaskBase>> Snapshot = ActiveTasks;
	for (UMultipleTaskBase* Task : Snapshot)
	{
		if (Task && !Task->bTaskEnded)
		{
			Task->OnActorOverlapEnd(OverlappedActor, OtherActor);
		}
	}
}

void UTaskSequencerComponent::ForwardCustomEvent(APlayerController* PC, FName EventId, const FString& Payload)
{
	if (!HasAuthorityForTask() || EventId.IsNone())
	{
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[TaskSequencer] CustomEvent forwarded: PC=%s event=%s payload len=%d"),
		*GetNameSafe(PC), *EventId.ToString(), Payload.Len());

	TArray<TObjectPtr<UMultipleTaskBase>> Snapshot = ActiveTasks;
	for (UMultipleTaskBase* Task : Snapshot)
	{
		if (Task && !Task->bTaskEnded)
		{
			Task->OnCustomEvent(PC, EventId, Payload);
		}
	}
}

// ==================== 辅助查询 ====================

APawn* UTaskSequencerComponent::GetPawnByRoleTag(FName RoleTag) const
{
	if (RoleTag.IsNone())
	{
		return nullptr;
	}

	// 真相源：AssignRole 写入的控制器映射
	if (const TWeakObjectPtr<APlayerController>* Found = RoleAssignments.Find(RoleTag))
	{
		if (APlayerController* PC = Found->Get())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				return Pawn;
			}
		}
	}

	// 兜底：直接按 Pawn 自身的 Tag 找。
	// 不能只经由 PlayerController（PC->GetPawn()）——测试关卡里 Pawn 可能
	// 尚未被 PC 占有或不在占有链上，但身份 Tag 就打在 Pawn 上。
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			APawn* Pawn = *It;
			if (Pawn && Pawn->ActorHasTag(RoleTag))
			{
				return Pawn;
			}
		}
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

ATaskAudioBroadcaster* UTaskSequencerComponent::GetAudioBroadcaster()
{
	// 已生成直接复用（服务器与客户端都可能有缓存，客户端侧不会走生成分支）
	if (AudioBroadcaster)
	{
		return AudioBroadcaster;
	}

	// 仅服务器侧生成（DS/Listen/单机）；客户端只接收 Multicast，不需要通道实例。
	// UWorld 上没有 AActor::HasAuthority，网络模式判定用 IsNetMode（PIE 与打包构建均正确）
	UWorld* World = GetWorld();
	if (!World || World->IsNetMode(NM_Client))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	AudioBroadcaster = World->SpawnActor<ATaskAudioBroadcaster>(ATaskAudioBroadcaster::StaticClass(), SpawnParams);
	if (AudioBroadcaster)
	{
		UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Spawned audio broadcaster %s"), *AudioBroadcaster->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[TaskSequencer] Failed to spawn audio broadcaster, client-side audio will be silent"));
	}
	return AudioBroadcaster;
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

	// 先进入"启动中"状态再清理与广播：后续对旧任务调 EndTask 会广播 OnTaskEnd，
	// bStepStarting 挡住 CheckStepCompletion 防止清理期间重入推进；
	// 同时防止监听者在回调里 Stop/Skip 与后续实例化交错
	bStepStarting = true;

	// 清理上一步的运行时实例：先结束残留任务——阻塞类早已自行 EndTask（幂等无副作用），
	// 实际被这里收尾的是纯展示/提示类非阻塞任务（UIPanelDisplay / WidgetDisplay），
	// 它们的 UI 组件在各自 EndTask 里统一销毁；
	// 同时作废上一步可能仍在挂起的步骤延迟定时器
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepStartDelayTimerHandle);
	}
	for (UMultipleTaskBase* OldTask : ActiveTasks)
	{
		if (OldTask)
		{
			OldTask->EndTask();
		}
	}
	for (UMultipleTaskBase* OldTask : ActiveTasks)
	{
		if (OldTask)
		{
			OldTask->OnTaskEnd.RemoveDynamic(this, &UTaskSequencerComponent::HandleTaskEnd);
		}
	}
	ActiveTasks.Empty();
	CurrentStepIndex = StepIndex;

	const FTaskStep& Step = CurrentTaskAsset->Steps[StepIndex];
	UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Step %d: %s (%d parallel tasks)"),
		StepIndex, *Step.StepTitle.ToString(), Step.ParallelTasks.Num());

	OnStepStarted.Broadcast(StepIndex);

	// 第一阶段：全部实例化并挂回调，此阶段不启动任何任务
	for (UMultipleTaskBase* SourceTask : Step.ParallelTasks)
	{
		if (!SourceTask)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TaskSequencer] Step %d has a null ParallelTasks entry, skipped"), StepIndex);
			continue;
		}

		// 配表里的对象归属 DataAsset，是共享模板；运行时必须实例化一份，
		// 否则多次训练会共用内部状态、并重复累积委托绑定
		UMultipleTaskBase* TaskInstance = DuplicateObject<UMultipleTaskBase>(SourceTask, this);
		if (!TaskInstance)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TaskSequencer] Step %d: DuplicateObject failed for task template, skipped"), StepIndex);
			continue;
		}

		TaskInstance->OnTaskEnd.Clear();
		TaskInstance->OwnerSequencer = this;
		TaskInstance->bTaskEnded = false;
		TaskInstance->OnTaskEnd.AddDynamic(this, &UTaskSequencerComponent::HandleTaskEnd);

		ActiveTasks.Add(TaskInstance);
	}

	// 第二阶段：整个步骤按配表延迟统一启动（DelayBeforeStart = 0 时立即启动）。
	// 延迟期间所有任务尚未 StartTask，阻塞任务未结束 → CheckStepCompletion 不会提前推进，
	// 即延迟自然计入步骤时间轴
	const float StepDelay = FMath::Max(Step.DelayBeforeStart, 0.f);
	UWorld* World = GetWorld();
	if (StepDelay > 0.f && World)
	{
		UE_LOG(LogTemp, Log, TEXT("[TaskSequencer] Step %d delayed %.2fs before start"), StepIndex, StepDelay);
		World->GetTimerManager().SetTimer(StepStartDelayTimerHandle, FTimerDelegate::CreateWeakLambda(this, [WeakThis = TWeakObjectPtr<UTaskSequencerComponent>(this)]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->StartPendingStepTasks();
			}
		}), StepDelay, false);
	}
	else
	{
		StartPendingStepTasks();
	}
	bStepStarting = false;
}

void UTaskSequencerComponent::StartPendingStepTasks()
{
	// 步骤延迟到期（或无延迟）：统一启动当前步骤已实例化的全部任务
	//（任务可能在 StartTask 内立刻结束，此时计数是完整的）
	for (UMultipleTaskBase* Task : ActiveTasks)
	{
		if (Task)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[TaskSequencer] Starting task: %s"), *Task->GetName());
			Task->StartTask();
		}
	}

	CheckStepCompletion();
}

void UTaskSequencerComponent::CheckStepCompletion()
{
	if (!bIsRunning || bStepStarting)
	{
		return;
	}

	// 只等阻塞任务结束——非阻塞任务（如 UIPanelDisplay）的存活不影响步骤推进
	int32 BlockingTasksRemaining = 0;
	for (UMultipleTaskBase* Task : ActiveTasks)
	{
		if (Task && Task->ShouldBlockStepCompletion() && !Task->bTaskEnded)
		{
			BlockingTasksRemaining++;
		}
	}

	if (BlockingTasksRemaining > 0)
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

		// 收尾：结束残留的非阻塞任务（纯展示面板/世界 Widget 提示等），
		// 让它们在 EndTask 里销毁自己的 UI；bIsRunning=false 已挡住 HandleTaskEnd 重入
		for (UMultipleTaskBase* Task : ActiveTasks)
		{
			if (Task)
			{
				Task->EndTask();
			}
		}

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

	UE_LOG(LogTemp, Verbose, TEXT("[TaskSequencer] Task ended: %s (blocking=%d)"),
		*EndedTask->GetName(), EndedTask->ShouldBlockStepCompletion() ? 1 : 0);
	CheckStepCompletion();
}
