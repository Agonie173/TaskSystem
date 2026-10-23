// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_PlayerListener.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "MultipleTask/TaskAudioBroadcaster.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

void UMultipleTask_PlayerListener::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerListener] StartTask failed: World is null"));
		EndTask();
		return;
	}

	if (RequiredPlayerCount < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerListener] RequiredPlayerCount < 1, task ends immediately"));
		EndTask();
		return;
	}

	if (BaseTagToAssign.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerListener] BaseTagToAssign is None, task ends immediately"));
		EndTask();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayerListener] Start listening: need %d players, base tag='%s', filter tags count=%d"),
		RequiredPlayerCount, *BaseTagToAssign.ToString(), FilterTags.Num());

	// 1) 初始扫描：把任务启动时已在场的玩家先处理一遍，避免错过
	for (TActorIterator<APlayerController> It(World); It; ++It)
	{
		APlayerController* PC = *It;
		if (PC)
		{
			ProcessPlayer(PC, PC->GetPawn());
		}
	}

	// 已满足则直接结束，不用等委托/轮询
	if (ProcessedPlayerIds.Num() >= RequiredPlayerCount)
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayerListener] Required count already met after initial scan, ending"));
		EndTask();
		return;
	}

	// 2) 绑定 Sequencer.OnPlayerRegistered 委托：新玩家 RegisterParticipant 成功时立即处理
	if (OwnerSequencer)
	{
		OwnerSequencer->OnPlayerRegistered.AddDynamic(this, &UMultipleTask_PlayerListener::HandlePlayerRegistered);
		bDelegateBound = true;
		UE_LOG(LogTemp, Log, TEXT("[PlayerListener] Bound to Sequencer.OnPlayerRegistered"));
	}
	else
	{
		// 没有 Sequencer 就退化为纯轮询模式（兼容测试关卡场景）
		UE_LOG(LogTemp, Warning, TEXT("[PlayerListener] OwnerSequencer is null, falling back to polling only"));
	}

	// 3) 启动兜底轮询：0.5s 扫一次世界，捕获委托未覆盖的 Pawn 延迟占有 / 未走 RegisterParticipant 等场景
	World->GetTimerManager().SetTimer(PollTimerHandle, this,
		&UMultipleTask_PlayerListener::PollTick, PollInterval, /*bLoop=*/true);
	UE_LOG(LogTemp, Log, TEXT("[PlayerListener] Poll started (interval=%.2fs)"), PollInterval);
}

void UMultipleTask_PlayerListener::EndTask_Implementation()
{
	// 先清理监听（委托 + 轮询），再调基类 EndTask——
	// 基类 EndTask 有 bTaskEnded 幂等保护，广播顺序由它负责
	CleanupListeners();

	UE_LOG(LogTemp, Log, TEXT("[PlayerListener] EndTask processed %d players (required %d)"),
		ProcessedPlayerIds.Num(), RequiredPlayerCount);

	Super::EndTask_Implementation();
}

void UMultipleTask_PlayerListener::CleanupListeners()
{
	// 解绑定 OnPlayerRegistered，防止任务结束后委托回调打到悬空对象
	if (bDelegateBound && OwnerSequencer)
	{
		OwnerSequencer->OnPlayerRegistered.RemoveDynamic(this, &UMultipleTask_PlayerListener::HandlePlayerRegistered);
		bDelegateBound = false;
	}

	// 停止兜底轮询
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimerHandle);
	}
	PollTimerHandle.Invalidate();
}

bool UMultipleTask_PlayerListener::PawnMatchesFilter(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}

	// FilterTags 为空时放行所有玩家
	if (FilterTags.Num() == 0)
	{
		return true;
	}

	for (const FName& Tag : FilterTags)
	{
		if (!Tag.IsNone() && Pawn->ActorHasTag(Tag))
		{
			return true;
		}
	}
	return false;
}

void UMultipleTask_PlayerListener::HandlePlayerRegistered(APlayerController* PC, APawn* Pawn)
{
	// Sequencer 保证 RegisterParticipant 里 PC 一定非空，但 Pawn 可能为空（尚未占有）
	ProcessPlayer(PC, Pawn);
}

void UMultipleTask_PlayerListener::PollTick()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<APlayerController> It(World); It; ++It)
	{
		APlayerController* PC = *It;
		if (PC)
		{
			ProcessPlayer(PC, PC->GetPawn());
		}
	}
}

void UMultipleTask_PlayerListener::ProcessPlayer(APlayerController* PC, APawn* Pawn)
{
	if (!PC || bTaskEnded)
	{
		return;
	}

	// Pawn 还没准备好：跳过，等下一次轮询或委托回调
	if (!Pawn)
	{
		return;
	}

	// 过滤标签校验
	if (!PawnMatchesFilter(Pawn))
	{
		return;
	}

	// 用 PlayerId 去重——Pawn 指针会因占有链变更而失效，但 PlayerId 是整局稳定的
	APlayerState* PS = PC->GetPlayerState<APlayerState>();
	if (!PS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerListener] PlayerState is null for %s, skipping"), *PC->GetName());
		return;
	}
	const int32 PlayerId = PS->GetPlayerId();
	if (ProcessedPlayerIds.Contains(PlayerId))
	{
		return;
	}

	// 生成编号标签：BaseTag + 自增序号（从 1 开始）
	const FName NumberedTag(*FString::Printf(TEXT("%s%d"),
		*BaseTagToAssign.ToString(), NextTagCounter));

	// 服务器本地打标签（AddUnique 防重复）
	if (!Pawn->Tags.Contains(NumberedTag))
	{
		Pawn->Tags.Add(NumberedTag);
	}

	// 广播到客户端同步标签（Actor 引用由引擎跨机解析同一复制实例）
	if (OwnerSequencer)
	{
		if (ATaskAudioBroadcaster* Broadcaster = OwnerSequencer->GetAudioBroadcaster())
		{
			Broadcaster->BroadcastModifyActorTags(Pawn, NumberedTag, /*bAdd=*/true);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerListener] Broadcaster not available, client tag sync skipped for %s"),
				*Pawn->GetName());
		}
	}

	// 记入已处理集合，计数器推进
	ProcessedPlayerIds.Add(PlayerId);
	NextTagCounter++;

	UE_LOG(LogTemp, Log, TEXT("[PlayerListener] Assigned tag '%s' to %s (playerId=%d, progress=%d/%d)"),
		*NumberedTag.ToString(), *Pawn->GetName(), PlayerId,
		ProcessedPlayerIds.Num(), RequiredPlayerCount);

	// 达到需求数量：结束任务
	if (ProcessedPlayerIds.Num() >= RequiredPlayerCount)
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayerListener] All %d required players processed, ending task"),
			RequiredPlayerCount);
		EndTask();
	}
}
