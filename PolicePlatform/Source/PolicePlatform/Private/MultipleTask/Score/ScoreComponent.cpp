// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/Score/ScoreComponent.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UScoreComponent::UScoreComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// ==================== 获取 ====================

UScoreComponent* UScoreComponent::FindScoreComponent(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->FindComponentByClass<UScoreComponent>() : nullptr;
}

UScoreComponent* UScoreComponent::FindOrCreateScoreComponent(UWorld* World)
{
	if (UScoreComponent* Existing = FindScoreComponent(World))
	{
		return Existing;
	}

	// 只在服务器创建（动态创建的复制组件会自动同步到客户端）
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	AGameStateBase* GameState = World->GetGameState();
	if (!GameState || !GameState->HasAuthority())
	{
		return nullptr;
	}

	UScoreComponent* NewComponent = NewObject<UScoreComponent>(GameState, TEXT("ScoreComponent"));
	if (!NewComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[Score] Failed to auto-create ScoreComponent on GameState %s"), *GetNameSafe(GameState));
		return nullptr;
	}

	NewComponent->RegisterComponent();
	UE_LOG(LogTemp, Log, TEXT("[Score] Auto-created ScoreComponent on GameState %s"), *GetNameSafe(GameState));
	return NewComponent;
}

// ==================== 评分 ====================

void UScoreComponent::ReportScoreEvent(FName RoleTag, FName ErrorCode)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (RoleTag.IsNone() || ErrorCode.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Score] ReportScoreEvent ignored: RoleTag or ErrorCode is None"));
		return;
	}

	const int32 Penalty = ResolvePenalty(ErrorCode);
	if (Penalty <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Score] Error code %s has no score rule in current task, recorded with 0 penalty"),
			*ErrorCode.ToString());
	}

	FPlayerScoreEntry* Entry = FindEntry(RoleTag);
	if (!Entry)
	{
		FPlayerScoreEntry NewEntry;
		NewEntry.RoleTag = RoleTag;
		NewEntry.FinalScore = BaseScore;
		ReplicatedScores.Add(NewEntry);
		Entry = &ReplicatedScores.Last();
	}

	Entry->TotalDeduction += Penalty;
	Entry->FinalScore = FMath::Max(0, BaseScore - Entry->TotalDeduction);
	Entry->ViolationCount++;
	Entry->ViolatedErrorCodes.AddUnique(ErrorCode);

	const int32 FinalScore = Entry->FinalScore;

	// 顺序稳定，HUD 刷新时不会跳行
	ReplicatedScores.Sort([](const FPlayerScoreEntry& A, const FPlayerScoreEntry& B)
	{
		return FNameLexicalLess()(A.RoleTag, B.RoleTag);
	});

	UE_LOG(LogTemp, Log, TEXT("[Score] %s reported %s (-%d), final %d"),
		*RoleTag.ToString(), *ErrorCode.ToString(), Penalty, FinalScore);
	// 补充累计信息，便于对账（角色/错误码/单次分值/累计扣分/违规次数）
	UE_LOG(LogTemp, Verbose, TEXT("[Score] %s totals: deduction %d, violations %d, error codes %d"),
		*RoleTag.ToString(), Entry->TotalDeduction, Entry->ViolationCount, Entry->ViolatedErrorCodes.Num());

	NotifyScoreChanged();
}

void UScoreComponent::ResetScores()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	ReplicatedScores.Reset();
	UE_LOG(LogTemp, Log, TEXT("[Score] All score entries reset"));
	NotifyScoreChanged();
}

int32 UScoreComponent::GetScore(FName RoleTag) const
{
	if (const FPlayerScoreEntry* Entry = FindEntry(RoleTag))
	{
		return Entry->FinalScore;
	}
	return BaseScore;
}

int32 UScoreComponent::GetTotalDeduction(FName RoleTag) const
{
	if (const FPlayerScoreEntry* Entry = FindEntry(RoleTag))
	{
		return Entry->TotalDeduction;
	}
	return 0;
}

// ==================== 内部 ====================

FPlayerScoreEntry* UScoreComponent::FindEntry(FName RoleTag)
{
	return ReplicatedScores.FindByPredicate([RoleTag](const FPlayerScoreEntry& Entry)
	{
		return Entry.RoleTag == RoleTag;
	});
}

const FPlayerScoreEntry* UScoreComponent::FindEntry(FName RoleTag) const
{
	return ReplicatedScores.FindByPredicate([RoleTag](const FPlayerScoreEntry& Entry)
	{
		return Entry.RoleTag == RoleTag;
	});
}

int32 UScoreComponent::ResolvePenalty(FName ErrorCode) const
{
	// 规则表在任务配表里，由同属 GameState 的 Sequencer 提供
	const AActor* Owner = GetOwner();
	const UTaskSequencerComponent* Sequencer =
		Owner ? Owner->FindComponentByClass<UTaskSequencerComponent>() : nullptr;
	if (!Sequencer)
	{
		return 0;
	}

	FScoreRule Rule;
	return Sequencer->FindScoreRule(ErrorCode, Rule) ? Rule.Penalty : 0;
}

void UScoreComponent::NotifyScoreChanged()
{
	if (const UWorld* World = GetWorld())
	{
		if (World->GetNetMode() != NM_DedicatedServer)
		{
			OnRep_Scores();
		}
	}
}

void UScoreComponent::OnRep_Scores()
{
	OnScoreBoardChanged.Broadcast();
}

void UScoreComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UScoreComponent, ReplicatedScores);
}
