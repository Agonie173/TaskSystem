// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTask_PanelBase.h"
#include "MultipleTask/Data/PrimaryTaskDataAsset.h"
#include "MultipleTask_EquipmentConfirm.generated.h"

/**
 * 自主选择并领取装备任务（05 步）：
 * 面板显示每名玩家当前已领取的装备（只统计 EquipmentTagPool 里的 Tag），
 * 三人分别点击【确认装备并继续】后，按 StandardEquipment 核对：
 * 少拿记 MissingEquipmentErrorCode，错拿/多拿记 WrongEquipmentErrorCode，
 * 每名玩家各记一次，不回退、不要求重选。
 *
 * 装备统一调整为标准装备由外层框架在步骤结束后完成，本任务不操作物品。
 *
 * 职责：展示各身份当前已领取的装备（按 EquipmentTagPool 过滤 Pawn Tag），
 *       收齐在座身份的确认回执后按 StandardEquipment 核对并上报评分事件。
 * 设计原因：领取动作由装备柜交互完成，本任务只读 Pawn Tag 做事后核对，不
 *       操作物品；确认制可避免玩家还没选完就被流程推进。
 * 用法：StandardEquipment 既是核对基准，也决定需要等哪些身份确认；缺席身份
 *       自动跳过（不参与确认也不评分），防止流程卡死。
 * 坑：仅服务器执行；EquipmentTagPool 为空时任何装备都识别不到，全员都会被判
 *       "少拿"；少拿/多拿各记一次分，记完不回退、不要求重选。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_EquipmentConfirm : public UMultipleTask_PanelBase
{
	GENERATED_BODY()

public:
	UMultipleTask_EquipmentConfirm();

	/** 任务开始：清空回执并下发面板；StandardEquipment 未配置时直接结束（仅服务器执行） */
	virtual void StartTask_Implementation() override;
	/** 组装面板数据：每个身份一行（当前装备 + 确认状态），并追加确认按钮 */
	virtual FTaskPanelData BuildPanelData() const override;
	/** 处理确认按钮回执：按 PlayerController 反查身份，去重记录并刷新面板，全员确认则核对结束 */
	virtual void HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex) override;

	// ==================== 配表 ====================

	/** 标准装备配置（核对基准，同时决定需要哪些身份确认） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TArray<FRoleEquipmentTags> StandardEquipment;

	/**
	 * 装备 Tag 池：只统计这些 Tag，避免把身份 Tag 等非装备 Tag 当成装备。
	 * 建议把装备柜里所有可选装备的 Tag 都列进来。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TArray<FName> EquipmentTagPool;

	/** 错误码：少拿装备 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FName MissingEquipmentErrorCode = FName(TEXT("EquipmentMissing"));

	/** 错误码：拿错或多拿装备 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FName WrongEquipmentErrorCode = FName(TEXT("EquipmentWrong"));

	// ==================== 面板 ====================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FName ConfirmButtonId = FName(TEXT("Confirm"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FText ConfirmButtonText;

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

	/** 某身份是否已确认 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	bool IsRoleConfirmed(FName RoleTag) const;

private:
	/** 已点击确认的身份 */
	UPROPERTY()
	TArray<FName> ConfirmedRoles;

	/** 该身份是否有玩家在场（缺席身份不参与确认与评分） */
	bool IsRolePresent(FName RoleTag) const;

	// 在座身份是否全部已确认（缺席身份自动豁免）
	bool AreAllRolesConfirmed() const;

	/** 核对并上报错误，然后结束 */
	void EvaluateAndFinish();
};
