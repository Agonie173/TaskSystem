// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_TagMonitor.generated.h"

class APawn;

/**
 * 标签监听任务：
 * 监听指定身份的 Pawn 是否贴上了指定标签，所有目标都贴上后任务结束。
 * 用于执法记录仪开启、装备穿戴确认等「做动作 → 贴标签 → 监听」的通用场景。
 *
 * 判定口径：低频轮询（CheckInterval 秒一次）读取 Pawn 的 Actor Tags，
 * 全部目标身份都贴上 RequiredTag 后 EndTask。
 * 轮询代替事件驱动的原因：装备/道具系统给 Pawn 贴标签没有统一事件回调，
 * 轮询实现简单且实时性足够（0.5s 间隔）。
 *
 * 用法：配置 MonitoredRoles（身份 → 要监听的标签列表）；
 * 可配置 bRequireAllRoles（true=所有身份都贴上才算完成，false=任意一个贴上即完成）。
 * 仅服务器执行，任务对象不存在于客户端。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_TagMonitor : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	/** 任务开始：校验配置并启动低频轮询（仅服务器执行） */
	virtual void StartTask_Implementation() override;
	/** 任务结束：清理轮询计时器（幂等，外部也可直接调用） */
	virtual void EndTask_Implementation() override;

	// ==================== 配置 ====================

	/** 身份 → 要监听的标签列表（该身份 Pawn 贴上其中任意一个标签即视为满足） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TagMonitor")
	TMap<FName, TArray<FName>> MonitoredRoles;

	/** true = 所有身份都满足才结束；false = 任意一个身份满足即结束 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TagMonitor")
	bool bRequireAllRoles = true;

	/** 轮询间隔（秒） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TagMonitor", meta = (ClampMin = "0.1"))
	float CheckInterval = 0.5f;

	// ==================== 查询 ====================

	/** 所有身份当前是否都已满足 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TagMonitor")
	bool AreAllRolesSatisfied() const;

	/** 某身份当前是否已满足 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TagMonitor")
	bool IsRoleSatisfied(FName RoleTag) const;

	/** 获取当前未满足的身份列表（调试/提示用） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TagMonitor")
	TArray<FName> GetUnsatisfiedRoles() const;

private:
	// 轮询计时器句柄（任务结束时清理）
	FTimerHandle CheckTimerHandle;

	// 轮询回调：按配置判定是否结束任务
	void CheckRequirements();

	// 判断单个身份的 Pawn 是否贴上了任一 RequiredTag（Pawn 缺席视为不满足）
	bool IsPawnTagged(APawn* Pawn, const TArray<FName>& RequiredTags) const;
};
