// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask/Data/PrimaryTaskDataAsset.h"
#include "MultipleTask_EquipmentInteract.generated.h"

/**
 * 装备检查任务：
 * 轮询参训玩家 Pawn 上的装备 Tag，所有身份的要求都满足后结束。
 * 领取装备（06 步）与装备清点（22 步）共用此任务，只改配表。
 *
 * 职责：作为"装备齐备门禁"——按 RoleTag → RequiredTags 逐一核对每个身份
 *       Pawn 的 Actor Tag，全部命中才放行，用于强制玩家完成取装备动作。
 * 设计原因：装备领取/放下只改 Pawn 的 Tag，没有事件回调，故用 CheckInterval
 *       低频轮询代替事件驱动，实现简单且实时性足够。
 * 用法：为每个身份配 RoleTag 与所需装备 Tag 集合；每 CheckInterval 秒检查一次，
 *       全部满足自动 EndTask。AreAllRequirementsMet / GetMissingTags 供 UI
 *       查询缺口。
 * 口径（以代码为准）：只检测"必需装备是否齐全"——少拿任一 Tag 就不满足，
 *       会一直等到补齐；代码不做"多拿"检测（多拿不算错），也不上报评分
 *       错误码。需要"少拿/多拿扣分"口径请用 MultipleTask_EquipmentConfirm。
 * 坑：仅服务器执行（Sequencer 服务器权威）；某身份 Pawn 缺席时该身份永远
 *       不满足，任务会一直等待；RequiredTags 里的 None Tag 会被跳过。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_EquipmentInteract : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	/** 任务开始：校验配表，若一开始就全部满足则立即结束，否则启动低频轮询（仅服务器执行） */
	virtual void StartTask_Implementation() override;
	/** 任务结束：先清掉轮询计时器再交由基类广播（幂等，外部也可直接调用） */
	virtual void EndTask_Implementation() override;

	// 每个身份需要的装备 Tag
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TArray<FRoleEquipmentTags> Requirements;

	// 轮询间隔（秒），Tag 变化没有事件通知，用低频轮询即可
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (ClampMin = "0.1"))
	float CheckInterval = 0.5f;

	/** 所有身份当前是否都满足了装备要求 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	bool AreAllRequirementsMet() const;

	/** 某身份当前缺少的装备 Tag（调试/提示用） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	TArray<FName> GetMissingTags(FName RoleTag) const;

private:
	// 低频轮询计时器句柄（任务结束时清理）
	FTimerHandle CheckTimerHandle;

	// 判断单个身份的 Pawn 是否带齐全部必需 Tag（Pawn 缺席视为不满足）
	bool IsRoleSatisfied(const FRoleEquipmentTags& Requirement) const;

	// 轮询回调：全部身份达标则结束任务，否则输出进度日志
	void CheckRequirements();
};
