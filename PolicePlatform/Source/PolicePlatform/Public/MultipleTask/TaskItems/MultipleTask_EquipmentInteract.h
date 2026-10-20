// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask/Data/PrimaryTaskDataAsset.h"
#include "MultipleTask_EquipmentInteract.generated.h"

class AActor;

/**
 * 装备交互任务：
 * 处理装备柜交互 → 自由领取 → 确认后记录并自动调整为标准装备
 * 也用于执法记录仪的开关交互
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_EquipmentInteract : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;

	// ==================== 装备柜配置 ====================

	// 装备柜 Actor 的 Tag
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EquipmentRack")
	FName EquipmentRackTag = FName(TEXT("EquipmentRack_Main"));

	// 装备柜里有哪些装备可选
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EquipmentRack")
	TArray<FEquipmentInfo> AvailableEquipments;

	// ==================== 标准装备映射 ====================

	// 确认后自动调整的标准装备（key=身份Tag，如 "Role_1"）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StandardEquipment")
	TMap<FName, FEquipmentInfo> StandardEquipmentByRoleTag;

	// 是否在确认后自动替换为标准装备
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StandardEquipment")
	bool bAutoAdjustAfterConfirm = true;

	// 是否记录玩家原始选择（用于评分）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StandardEquipment")
	bool bRecordOriginalChoice = true;

	// ==================== 确认按钮 ====================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Confirm")
	FText ConfirmButtonText = FText::FromString(TEXT("确认装备"));

	// 确认所需身份Tag（空=所有人都可以点）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Confirm")
	FName ConfirmRoleTag = NAME_None;

	// ==================== 对外接口 ====================

	// 玩家选择了某装备（蓝图/UI 回调）
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void ReportEquipmentSelected(FName PlayerRoleTag, const FEquipmentInfo& SelectedEquipment);

	// 玩家点击确认（蓝图/UI 回调）
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void ReportConfirmClicked(FName PlayerRoleTag);

private:
	// 记录每个玩家的原始选择（用于评分）
	TMap<FName, FEquipmentInfo> OriginalChoices;

	bool bConfirmReceived = false;

	void DoAutoAdjust();
	void CheckCompletion();
};
