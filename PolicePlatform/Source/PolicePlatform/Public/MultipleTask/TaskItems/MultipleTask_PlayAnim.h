// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_PlayAnim.generated.h"

class APawn;

// 单个动画播放配置
USTRUCT(Blueprintable, BlueprintType)
struct FMultipleTaskPlayAnimInfo
{
	GENERATED_BODY()

	// 要播放的动画
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequenceBase* PlayAnimSequence = nullptr;
};

/**
 * 动画播放任务：在指定Pawn/NPC上播放单个或多个动画
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_PlayAnim : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;

	// 是否需要多动画序列
	UPROPERTY(EditDefaultsOnly)
	bool bNeedMultipleAnim = false;

	// 单个动画播放
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "bNeedMultipleAnim == false", EditConditionHides))
	FMultipleTaskPlayAnimInfo PlayAnimInfo;

	// 多个动画播放
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "bNeedMultipleAnim == true", EditConditionHides))
	TArray<FMultipleTaskPlayAnimInfo> PlayAnimInfoArray;

	// 按身份Tag查找目标Pawn（空=直接用 TargetPawnRef）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target")
	FName TargetRoleTag = NAME_None;

	// 直接引用Pawn（用于NPC）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target",
		meta = (EditCondition = "TargetRoleTag == NAME_None", EditConditionHides))
	TSoftObjectPtr<APawn> TargetPawnRef;

	// 是否等待动画播完再结束任务
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Playback")
	bool bWaitAnimEnd = true;

	// 循环次数（1=单次，0=无限，正整数=N次）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Playback")
	int32 LoopCount = 1;

private:
	UPROPERTY()
	TObjectPtr<APawn> CachedTargetPawn;

	int32 CurrentAnimIndex = 0;
	int32 CurrentLoopCount = 0;

	void PlayNextAnim();

	UFUNCTION()
	void OnSingleAnimEnd();
};
