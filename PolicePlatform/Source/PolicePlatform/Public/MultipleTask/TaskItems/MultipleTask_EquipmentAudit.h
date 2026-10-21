// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTask_PanelBase.h"
#include "MultipleTask/Data/PrimaryTaskDataAsset.h"
#include "MultipleTask_EquipmentAudit.generated.h"

/**
 * 装备核对任务：
 * 按 StandardEquipment 核对每个身份的装备是否正确（少拿/多拿/拿错），
 * 有错误则上报评分事件扣分。不等待玩家确认，启动后立即核对并结束。
 *
 * 职责：读取各身份 Pawn 上的装备标签，与 StandardEquipment 对比，
 *       上报 MissingEquipmentErrorCode / WrongEquipmentErrorCode 评分事件。
 * 设计原因：与 EquipmentConfirm 分离——确认任务只管"等确认+锁定"，
 *       核对任务只管"检查+扣分"，复用性更好。
 * 用法：在 EquipmentConfirm 之后的步骤中配置；缺席身份不参与核对。
 * 坑：仅服务器执行；核对时装备应已被锁定（EquipmentLockedTag），否则核对结果可能不准。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_EquipmentAudit : public UMultipleTask_PanelBase
{
	GENERATED_BODY()

public:
	/** 启动任务：立即执行核对、上报评分并结束 */
	virtual void StartTask_Implementation() override;

	// ==================== 配表 ====================

	/** 标准装备配置（核对基准） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TArray<FRoleEquipmentTags> StandardEquipment;

	/** 装备 Tag 池：只统计这些 Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TArray<FName> EquipmentTagPool;

	/** 错误码：少拿装备 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FName MissingEquipmentErrorCode = FName(TEXT("EquipmentMissing"));

	/** 错误码：拿错或多拿装备 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FName WrongEquipmentErrorCode = FName(TEXT("EquipmentWrong"));

	// ==================== 查询 ====================

	/** 某身份当前已领取的装备 Tag */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	TArray<FName> GetSelectedEquipment(FName RoleTag) const;

	/** 某身份相对标准配置缺少的装备 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	TArray<FName> GetMissingEquipment(FName RoleTag) const;

	/** 某身份相对标准配置多拿/拿错的装备 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	TArray<FName> GetWrongEquipment(FName RoleTag) const;

private:
	/** 执行核对并上报评分 */
	void EvaluateAndReport();

	/** 该身份是否有玩家在场 */
	bool IsRolePresent(FName RoleTag) const;
};
