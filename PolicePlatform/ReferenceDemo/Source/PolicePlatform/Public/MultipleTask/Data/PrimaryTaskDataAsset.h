// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PrimaryTaskDataAsset.generated.h"

class UMultipleTaskBase;

/**
 * 一个身份 + 该身份要站的碰撞盒（多人到位 / 就位确认共用）
 */
USTRUCT(BlueprintType)
struct FRoleStationPair
{
	GENERATED_BODY()

	// 要求到位的身份 Tag（如 "Role_1"）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stations")
	FName RoleTag;

	// 关卡中站位碰撞盒 Actor 的 Tag
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stations")
	FName StationTag;
};

/**
 * 某个身份需要携带的装备 Tag 列表（领装 / 装备清点共用）。
 * 物品被拿取时由物品类给拾取者 Pawn 贴 Tag，丢弃时移除；
 * 任务只读 Pawn 上的 Tag，不关心装备 Actor 类。
 */
USTRUCT(BlueprintType)
struct FRoleEquipmentTags
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FName RoleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TArray<FName> RequiredTags;
};

/**
 * 评分规则：错误码 → 扣分值
 * 任务只上报错误码，由评分组件按当前步骤的规则扣分，任务本身不关心分值
 */
USTRUCT(BlueprintType)
struct FScoreRule
{
	GENERATED_BODY()

	// 错误码（任务里配置的 ViolationErrorCode 就是它）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName ErrorCode;

	// 描述（评分报表/调试用）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText Description;

	// 扣分值（正数）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 Penalty = 0;
};

/**
 * 单个任务步骤：
 * Step 内的所有 ParallelTasks 并行执行，全部完成后进入下一个 Step
 */
USTRUCT(BlueprintType)
struct FTaskStep
{
	GENERATED_BODY()

	// 步骤ID（如 "Step_01"）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName StepId;

	// 步骤标题（调试/UI 用）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText StepTitle;

	// 本步骤要并行启动的所有任务项，全部完成后才进入下一步
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced,
		meta = (ShowOnlyInnerProperties))
	TArray<TObjectPtr<UMultipleTaskBase>> ParallelTasks;

	// 本步骤开始前整体延迟（秒），步骤内所有任务到点后统一启动。
	// 0 = 立即开始；用于步骤之间留白（如上个动作完成停顿几秒再进入下一步）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float DelayBeforeStart = 0.f;

	// 本步骤涉及的评分规则（错误码 → 扣分值）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ShowOnlyInnerProperties))
	TArray<FScoreRule> ScoreRules;
};

/**
 * 主任务配表：一个任务 = 多个步骤顺序执行
 * 每个步骤 = 多个任务项并行执行
 *
 * 三层结构：Steps（顺序步骤）→ 每步的 ParallelTasks（Instanced 并行任务项）
 * + ScoreRules（本步骤的错误码 → 扣分规则）。
 *
 * 重要——本资产是“模板对象”：
 *   UPROPERTY 用 Instanced 修饰 ParallelTasks，编辑器里每个任务项都是
 *   独立实例；但运行时绝对不允许直接执行模板里的任务对象，否则多个
 *   训练会话/多次重开会共享同一份任务实例状态（进度、标记位互相污染），
 *   且改动会写回资产。正确做法是 Sequencer 在 ExecuteStep 时用
 *   DuplicateObject 把当前步骤的任务项实例化成运行时副本后再启动。
 *   同理，任何运行时代码都不得修改本资产（只读使用）。
 */
UCLASS()
class POLICEPLATFORM_API UPrimaryTaskDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 任务名（日志/调试/任务列表显示用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	FName TaskName;

	// 任务开始所需的最少玩家数（含 Listen Server 的 host，DS 即远程客户端总数）。
	// 0 = 不主动要求人数（仍会等首个玩家进场）；多人局设为期望总人数，
	// 避免任务在部分玩家连接完成前启动——复制通道未建立时音频/动画/面板广播会被静默丢弃
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Task", meta = (ClampMin = "0"))
	int32 MinPlayersToStart = 0;

	// 所有步骤，顺序执行；每个 Step 内的 ParallelTasks 并行执行
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steps",
		meta = (ShowOnlyInnerProperties))
	TArray<FTaskStep> Steps;
};
