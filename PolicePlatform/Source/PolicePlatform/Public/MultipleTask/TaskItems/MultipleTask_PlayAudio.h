// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_PlayAudio.generated.h"

class APawn;
class UAnimSequence;
class USoundBase;
class UAudioComponent;

UENUM(BlueprintType)
enum class EAudioPlayLocationType : uint8
{
	PlayAudioForLocation, // 在世界固定坐标播放（不跟随任何对象）
	PlayAudioForPawn      // 挂在某个 Pawn 身上播放（跟随其移动）
};

USTRUCT(BlueprintType, Blueprintable)
struct FPlayAudioTaskInfo
{
	GENERATED_BODY()

	// 要播放的音频
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundBase* PlaySound = nullptr;

	// 播放位置类型
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EAudioPlayLocationType PlayAudioType = EAudioPlayLocationType::PlayAudioForLocation;

	// 音频播放位置（PlayAudioForLocation 时使用）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "PlayAudioType == EAudioPlayLocationType::PlayAudioForLocation", EditConditionHides))
	FVector PlayAudioLocation = FVector::ZeroVector;

	// 按身份Tag查找目标Pawn（空=直接用 PlayAudioPawn）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "PlayAudioType == EAudioPlayLocationType::PlayAudioForPawn", EditConditionHides))
	FName TargetRoleTag = NAME_None;

	// 直接引用Pawn（用于NPC或非玩家对象）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "PlayAudioType == EAudioPlayLocationType::PlayAudioForPawn && TargetRoleTag == NAME_None",
			EditConditionHides))
	TSoftObjectPtr<APawn> PlayAudioPawn = nullptr;

	// 在Pawn上播放时是否需要播放动画
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "PlayAudioType == EAudioPlayLocationType::PlayAudioForPawn", EditConditionHides))
	bool bNeedPlayAnim = false;

	// 需要播放的动画序列
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "PlayAudioType == EAudioPlayLocationType::PlayAudioForPawn && bNeedPlayAnim == true",
			EditConditionHides))
	UAnimSequence* PlayAudioSequence = nullptr;

	// 播放前延迟（秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float DelayBeforePlayback = 0.0f;

	// 播放后延迟（秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float DelayAfterPlayback = 0.0f;
};

/**
 * 音频播放任务：在指定位置或 Pawn 上播放音频，可关联动画。
 *
 * 结构：PlayAudioTaskInfo 是一个按顺序播放的"清单"，每条配置支持播放前/后延时；
 * 上一条播完（OnAudioFinished 回调驱动，而非固定时长定时器）才推进下一条，
 * 全部播完后任务结束。
 *
 * PlayAudioForPawn：在目标 Pawn 身上找 UAudioComponent，没有就动态 NewObject 一个
 * （命名 AutoAudioComp）挂到 Pawn 根组件下再注册，随后 SetSound + Play；
 * 目标解析优先 TargetRoleTag（经 Sequencer 角色分配表），其次 PlayAudioPawn 直接引用；
 * 可选同时播一段动画（bNeedPlayAnim）。
 * PlayAudioForLocation：用 SpawnSoundAtLocation 在固定位置生成临时音频组件（bAutoDestroy）。
 *
 * 坑（网络语义）：播放只在服务器触发。虽然动态 AudioComponent 调用了
 * SetIsReplicated(true)，但组件复制并不会让客户端"响起声音"——音频播放本身
 * 没有 RPC/属性同步驱动，DS 下客户端听不到（已知限制），仅服务器本地可闻。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_PlayAudio : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	/** 任务开始：从头开始逐条播放音频清单（仅服务器） */
	virtual void StartTask_Implementation() override;
	/** 任务结束：清理延时定时器、解绑并停止当前音频组件 */
	virtual void EndTask_Implementation() override;

	// 播放清单（按顺序逐条播放）
	UPROPERTY(EditDefaultsOnly)
	TArray<FPlayAudioTaskInfo> PlayAudioTaskInfo;

private:
	// 播放序号
	int32 PlayID = 0;

	// 当前正在使用的 AudioComponent（防止 GC）
	UPROPERTY()
	TObjectPtr<UAudioComponent> CurrentAudioComponent;

	/** 播放前/后延时定时器（成员持有，避免任务结束后回调野指针） */
	FTimerHandle DelayTimerHandle;

	/** 开始播放当前条目（含播放前延时的调度） */
	void StartPlayAudio();

	/** 在 Pawn 的骨骼网格上播放音频配套动画（无骨骼组件时静默跳过） */
	void PlayAnim(APawn* Pawn, UAnimSequence* PlayAudioSequence);

	/** 在目标 Pawn 身上播放（查找/动态创建 AudioComponent），播完走 AudioFinished */
	void PlayAudioForPawn();

	/** 在世界固定位置播放（SpawnSoundAtLocation 临时组件，自动销毁） */
	void PlayAudioForLocation();

	// 找到目标Pawn（优先 TargetRoleTag，其次 PlayAudioPawn）
	APawn* ResolveTargetPawn() const;

	/** 单条音频播完回调：清理引用并推进播放清单（含播放后延时） */
	UFUNCTION()
	void AudioFinished();

	// 延迟回调
	// 播放前延时到期：真正开始播放
	void OnDelayBeforePlayback();
	// 播放后延时到期：开始下一条
	void OnDelayAfterPlayback();
};
