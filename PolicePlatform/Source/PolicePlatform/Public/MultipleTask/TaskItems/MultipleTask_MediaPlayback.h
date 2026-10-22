// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_MediaPlayback.generated.h"

class APawn;
class UAnimSequenceBase;
class USoundBase;
class UAudioComponent;

UENUM(BlueprintType)
enum class EMediaPlayLocationType : uint8
{
	AtLocation, // 在世界固定坐标播放
	OnPawn      // 挂在某个 Pawn 身上播放
};

/**
 * 媒体播放项：一条音频+动画的组合配置
 */
USTRUCT(BlueprintType)
struct FMediaPlaybackItem
{
	GENERATED_BODY()

	// 要播放的音频（可选）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundBase* Sound = nullptr;

	// 要播放的动画（可选）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequenceBase* AnimSequence = nullptr;

	// 播放位置类型
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EMediaPlayLocationType LocationType = EMediaPlayLocationType::AtLocation;

	// 世界固定坐标（LocationType = AtLocation 时使用）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "LocationType == EMediaPlayLocationType::AtLocation", EditConditionHides))
	FVector Location = FVector::ZeroVector;

	// 按身份Tag查找目标Pawn（LocationType = OnPawn 时使用）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "LocationType == EMediaPlayLocationType::OnPawn", EditConditionHides))
	FName TargetRoleTag = NAME_None;

	// 直接引用Pawn（LocationType = OnPawn 且 TargetRoleTag 为空时使用）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "LocationType == EMediaPlayLocationType::OnPawn && TargetRoleTag == NAME_None", EditConditionHides))
	TSoftObjectPtr<APawn> TargetPawnRef = nullptr;

	// 播放前延迟（秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float DelayBefore = 0.0f;

	// 播放后延迟（秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float DelayAfter = 0.0f;

	// 是否等待动画播完（仅动画有效）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bWaitAnimEnd = true;

	// 动画循环次数（0=无限，1=单次，正整数=N次）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 LoopCount = 1;
};

/**
 * 媒体播放任务：
 * 合并原 PlayAnim 和 PlayAudio 的功能，支持音频、动画或两者同时播放。
 *
 * 播放模式：
 * - 清单模式：多条按顺序播放，上一条完成后才播下一条
 * - 单条模式：只播放一条
 *
 * 目标解析：优先按 TargetRoleTag 经 Sequencer 查找 Pawn，其次用 TargetPawnRef 直接引用。
 *
 * 完成条件：全部清单播放完成后结束任务。
 * 音频完成 = OnAudioFinished 回调；动画完成 = 播放时长定时器（GetPlayLength）。
 *
 * 网络语义：播放只在服务器触发，动画/音频状态无同步驱动，DS 下客户端看不到/听不到（已知限制）。
 * 仅适合服务器本地演示或单机场景。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_MediaPlayback : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	/** 任务开始：从头开始逐条播放媒体清单 */
	virtual void StartTask_Implementation() override;
	/** 任务结束：清理定时器、停止音频组件 */
	virtual void EndTask_Implementation() override;

	// 播放清单（按顺序逐条播放）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Media")
	TArray<FMediaPlaybackItem> Playlist;

private:
	// 当前播放到的条目下标
	int32 CurrentIndex = 0;

	// 当前使用的 AudioComponent（防止 GC）
	UPROPERTY()
	TObjectPtr<UAudioComponent> CurrentAudioComponent;

	// 当前目标 Pawn（OnPawn 模式）
	UPROPERTY()
	TObjectPtr<APawn> CurrentTargetPawn;

	// 延时定时器
	FTimerHandle DelayTimerHandle;

	// 动画时长定时器
	FTimerHandle AnimTimerHandle;

	// 当前是否处于播放前延迟阶段（区分播放前/后延迟）
	bool bIsPreDelay = false;

	// 当前音频是否已完成
	bool bAudioFinished = false;

	// 当前动画是否已完成
	bool bAnimFinished = false;

	// 开始播放当前条目
	void PlayCurrentItem();

	// 播放音频（AtLocation / OnPawn）
	void PlayAudio(const FMediaPlaybackItem& Item);
	void PlayAudioAtLocation(USoundBase* Sound, const FVector& Location);
	void PlayAudioOnPawn(USoundBase* Sound, APawn* Pawn);

	// 播放动画（OnPawn）
	void PlayAnimOnPawn(UAnimSequenceBase* Anim, APawn* Pawn, int32 LoopCount);

	// 查找目标 Pawn
	APawn* ResolveTargetPawn(const FMediaPlaybackItem& Item) const;

	// 单条播放完成（音频/动画/延时）
	void OnItemFinished();

	// 播放下一条
	void PlayNext();

	// 推进到下一条（清理当前 + 递增索引 + 播放）
	void AdvanceToNext();

	// 音频播放完成回调
	UFUNCTION()
	void OnAudioFinished();

	// 动画时长到期回调
	UFUNCTION()
	void OnAnimTimerEnd();

	// 延时到期回调
	UFUNCTION()
	void OnDelayTimerEnd();

	// 清理当前播放状态
	void CleanupCurrentPlayback();
};
