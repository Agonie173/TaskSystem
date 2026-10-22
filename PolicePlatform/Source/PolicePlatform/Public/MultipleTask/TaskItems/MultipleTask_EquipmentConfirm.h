// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTask_PanelBase.h"
#include "MultipleTask/Data/PrimaryTaskDataAsset.h"
#include "MultipleTask_EquipmentConfirm.generated.h"

/**
 * 自主选择并领取装备确认任务：
 * 面板显示每名玩家当前已领取的装备（只统计 EquipmentTagPool 里的 Tag），
 * 三人分别点击【确认装备并继续】后，给各自 Pawn 贴上 EquipmentLockedTag 锁定装备，
 * 全员确认后任务结束。核对扣分由独立的 EquipmentAudit 任务执行。
 *
 * 职责：展示各身份当前已领取的装备（按 EquipmentTagPool 过滤 Pawn Tag），
 *       收齐在座身份的确认回执后给各自 Pawn 贴锁定标签，然后结束任务。
 * 设计原因：确认与核对分离——确认任务只负责"等确认+锁定"，核对任务只负责"检查+扣分"，
 *       复用性更好。确认后锁定可防止玩家在核对前换装导致核对结果不准。
 * 用法：StandardEquipment 决定需要等哪些身份确认；缺席身份自动跳过（不参与确认）。
 * 坑：仅服务器执行；确认后 Pawn 会被贴上 EquipmentLockedTag，装备系统可检查此标签禁止再换装。
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
	/** 处理确认按钮回执：按 PlayerController 反查身份，去重记录、贴锁定标签并刷新面板，全员确认则结束 */
	virtual void HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex) override;

	// ==================== 配表 ====================

	/** 标准装备配置（决定需要等哪些身份确认） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TArray<FRoleEquipmentTags> StandardEquipment;

	/**
	 * 装备 Tag 池：只统计这些 Tag，避免把身份 Tag 等非装备 Tag 当成装备。
	 * 建议把装备柜里所有可选装备的 Tag 都列进来。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TArray<FName> EquipmentTagPool;

	// ==================== 面板 ====================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FName ConfirmButtonId = FName(TEXT("Confirm"));

	/** 确认后贴到 Pawn 上的锁定标签（装备系统可检查此标签禁止再换装） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FName EquipmentLockedTag = TEXT("EquipmentLocked");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FText ConfirmButtonText;

	// ==================== 查询 ====================

	/** 某身份当前已领取的装备 Tag */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	TArray<FName> GetSelectedEquipment(FName RoleTag) const;

	/** 某身份是否已确认 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	bool IsRoleConfirmed(FName RoleTag) const;

private:
	/** 已点击确认的身份 */
	UPROPERTY()
	TArray<FName> ConfirmedRoles;

	/** 该身份是否有玩家在场（缺席身份不参与确认） */
	bool IsRolePresent(FName RoleTag) const;

	// 在座身份是否全部已确认（缺席身份自动豁免）
	bool AreAllRolesConfirmed() const;
};
