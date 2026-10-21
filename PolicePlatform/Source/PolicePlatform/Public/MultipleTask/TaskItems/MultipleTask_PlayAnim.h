// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_PlayAnim.generated.h"

class APawn;
class UAnimSequenceBase;

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
 * 动画播放任务：在指定 Pawn/NPC 上播放单个或多个动画，播完或延时后结束任务。
 *
 * 目标解析：优先按 TargetRoleTag（经 Sequencer 的角色分配表）查 Pawn，
 * 否则用 TargetPawnRef 直接引用（适合 NPC）；目标在 StartTask 时解析并缓存到
 * CachedTargetPawn，任务期间不重复查找。
 *
 * 播放实现：直接在目标骨骼网格组件上 PlayAnimation（AnimSequence / AnimMontage 均可），
 * 结束时机 = 动画播放时长定时器（GetPlayLength），不支持动画结束通知的精确回调；
 * 多动画按数组顺序接续播放，LoopCount 控制整组循环次数（0 = 无限循环）。
 * bWaitAnimEnd = false 时动画照常触发但任务立即结束（不等播完）。
 *
 * 坑（网络语义）：播放只在服务器逻辑里发生，动画状态没有任何同步驱动，
 * DS 下客户端看不到该动画（已知限制）；仅适合服务器本地演示或单机场景。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_PlayAnim : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	/** 任务开始：解析目标 Pawn 并开始播放动画序列（仅服务器） */
	virtual void StartTask_Implementation() override;
	/** 任务结束：清理动画时长定时器 */
	virtual void EndTask_Implementation() override;

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
	/** StartTask 时解析并缓存的目标 Pawn（任务期间有效，避免重复查找） */
	UPROPERTY()
	TObjectPtr<APawn> CachedTargetPawn;

	// 当前播放到的动画下标（多动画序列用）
	int32 CurrentAnimIndex = 0;
	// 已完成的整组循环轮数（与 LoopCount 比较）
	int32 CurrentLoopCount = 0;

	/** 动画播放时长定时器（成员持有，EndTask 时清理） */
	FTimerHandle AnimTimerHandle;

	/** 播放下一个动画段（含循环推进），目标/资产缺失或播放失败即结束任务 */
	void PlayNextAnim();

	/** 在目标 Pawn 上播放一段动画，返回是否成功播放 */
	bool PlayAnimOnTarget(UAnimSequenceBase* AnimToPlay);

	/** 单段动画时长到期回调：推进序列或循环轮次 */
	UFUNCTION()
	void OnSingleAnimEnd();
};
