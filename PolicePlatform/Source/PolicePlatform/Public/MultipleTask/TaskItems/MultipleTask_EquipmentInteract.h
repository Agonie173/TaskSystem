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
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_EquipmentInteract : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
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
	FTimerHandle CheckTimerHandle;

	bool IsRoleSatisfied(const FRoleEquipmentTags& Requirement) const;

	void CheckRequirements();
};
