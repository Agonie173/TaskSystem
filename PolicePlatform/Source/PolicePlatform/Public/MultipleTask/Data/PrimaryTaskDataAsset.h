// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PrimaryTaskDataAsset.generated.h"

class UMultipleTaskBase;

/**
 * 装备信息（配表用）
 */
USTRUCT(BlueprintType)
struct FEquipmentInfo
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName EquipmentId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftClassPtr<AActor> EquipmentClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor Color = FLinearColor::White;
};

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

	// 本步骤涉及的评分规则（错误码 → 扣分值）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ShowOnlyInnerProperties))
	TArray<FScoreRule> ScoreRules;
};

/**
 * 主任务配表：一个任务 = 多个步骤顺序执行
 * 每个步骤 = 多个任务项并行执行
 */
UCLASS()
class POLICEPLATFORM_API UPrimaryTaskDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	FName TaskName;

	// 身份Tag → 初始标准装备映射（Sequencer 初始化时用）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup")
	TMap<FName, FEquipmentInfo> DefaultEquipmentByRoleTag;

	// 所有步骤，顺序执行；每个 Step 内的 ParallelTasks 并行执行
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steps",
		meta = (ShowOnlyInnerProperties))
	TArray<FTaskStep> Steps;
};
