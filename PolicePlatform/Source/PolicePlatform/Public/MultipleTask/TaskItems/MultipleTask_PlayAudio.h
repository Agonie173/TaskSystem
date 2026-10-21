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
	PlayAudioForLocation,
	PlayAudioForPawn
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
 * 音频播放任务：在指定位置或Pawn上播放音频，可关联动画
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_PlayAudio : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void EndTask_Implementation() override;

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

	void StartPlayAudio();
	void PlayAnim(APawn* Pawn, UAnimSequence* PlayAudioSequence);
	void PlayAudioForPawn();
	void PlayAudioForLocation();

	// 找到目标Pawn（优先 TargetRoleTag，其次 PlayAudioPawn）
	APawn* ResolveTargetPawn() const;

	UFUNCTION()
	void AudioFinished();

	// 延迟回调
	void OnDelayBeforePlayback();
	void OnDelayAfterPlayback();
};
