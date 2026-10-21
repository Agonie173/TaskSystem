// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTask_PanelBase.h"
#include "MultipleTask_WaitForConfirm.generated.h"

class APlayerController;

/**
 * 确认策略：谁需要点
 */
UENUM(BlueprintType)
enum class EConfirmPolicy : uint8
{
	AllPlayers,       // 所有参训玩家都要确认
	AnyPlayer,        // 任意一个玩家确认即结束
	SpecificRoles,    // 指定身份的玩家确认
	FirstRole_1,      // 仅 Role_1 确认（常用在指挥/上报场景）
};

/**
 * 等待玩家确认任务：
 * 通过面板通道下发确认按钮，等待满足策略后结束。
 * 面板显示方式完全交给外层框架，不区分 PC/VR。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_WaitForConfirm : public UMultipleTask_PanelBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual FTaskPanelData BuildPanelData() const override;
	virtual void HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex) override;

	// 确认按钮文字（为空则不生成按钮）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText ConfirmButtonText;

	// ==================== 确认策略 ====================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Policy")
	EConfirmPolicy ConfirmPolicy = EConfirmPolicy::AllPlayers;

	// Policy=SpecificRoles 时，指定哪些身份需要确认
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Policy",
		meta = (EditCondition = "ConfirmPolicy == EConfirmPolicy::SpecificRoles", EditConditionHides))
	TArray<FName> SpecificRoleTags;

	// ==================== 外部确认接口（服务器调用） ====================

	/** 某个玩家确认了（按身份Tag，需已分配身份） */
	UFUNCTION(BlueprintCallable, Category = "WaitForConfirm")
	void ReportPlayerConfirmed(FName PlayerRoleTag);

	/** 某个玩家确认了（按 PlayerController，未分配身份时也可用） */
	UFUNCTION(BlueprintCallable, Category = "WaitForConfirm")
	void ReportPlayerConfirmedByController(APlayerController* PC);

private:
	// 记录哪些玩家已确认（身份维度，用于 SpecificRoles / FirstRole_1）
	TSet<FName> ConfirmedRoleTags;

	// 记录哪些玩家已确认（连接维度，用于 AllPlayers）
	TSet<TWeakObjectPtr<APlayerController>> ConfirmedControllers;

	bool IsConfirmedBy(APlayerController* PC) const;

	// 检查确认条件是否满足
	void CheckCompletion();
};
