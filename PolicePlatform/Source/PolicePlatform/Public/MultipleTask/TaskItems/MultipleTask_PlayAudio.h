// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_PlayAudio.generated.h"

class APawn;
class ATaskAudioBroadcaster;
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
 * 音频播放任务：按清单顺序逐条播放（OnAudioFinished 驱动，条目支持前后延时），全部播完结束。
 * 服务器 AudioComponent 仅驱动任务流；声音与说话动画经广播通道 Multicast 到客户端重建，
 * EndTask 凭句柄广播停止客户端音频。音频属瞬态表现，中途加入不补播。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_PlayAudio : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void EndTask_Implementation() override;

	// 播放清单（按顺序逐条播放）
	UPROPERTY(EditDefaultsOnly)
	TArray<FPlayAudioTaskInfo> PlayAudioTaskInfo;

private:
	// 播放序号
	int32 PlayID = 0;

	// 最近一次广播播放的句柄（EndTask 时凭此广播停止客户端音频）
	int32 LastAudioHandle = INDEX_NONE;

	// 当前正在使用的 AudioComponent（防止 GC）
	UPROPERTY()
	TObjectPtr<UAudioComponent> CurrentAudioComponent;

	// 条目播放前/后延时定时器（EndTask 统一清理）
	FTimerHandle DelayTimerHandle;

	// 取广播通道（OwnerSequencer 为空时返回 null）
	ATaskAudioBroadcaster* GetBroadcaster() const;
	// 切换委托绑定的音频组件（复用已有组件时防重复绑定）
	void BindAudioFinished(UAudioComponent* AudioComp);
	// 播放当前条目：按播放前延时调度或直接开始
	void StartPlayAudio();
	// 当前条目真正开始播放（按 PlayAudioType 分派）
	void PlayCurrentItem();
	// 在目标 Pawn 身上播放（查找或动态创建 AudioComponent）
	void PlayAudioForPawn();
	// 在世界固定位置播放（临时组件，自动销毁）
	void PlayAudioForLocation();
	// 经广播通道在目标 Pawn 上播放说话动画（Slot 蒙太奇，自动混合回蓝图）
	void PlayAnim(APawn* Pawn, UAnimSequence* PlayAudioSequence);
	// 找到目标Pawn（优先 TargetRoleTag，其次 PlayAudioPawn）
	APawn* ResolveTargetPawn() const;
	// 单条音频播完：清理引用并推进清单（含播放后延时）
	UFUNCTION()
	void AudioFinished();
};
