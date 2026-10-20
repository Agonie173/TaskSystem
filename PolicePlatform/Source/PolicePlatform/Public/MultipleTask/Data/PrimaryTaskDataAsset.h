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
