// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask/UI/TaskPanelTypes.h"
#include "MultipleTask_PanelBase.generated.h"

class APlayerController;
class UTaskPanelChannelComponent;

/** 面板下发给谁 */
UENUM(BlueprintType)
enum class EPanelAudience : uint8
{
	AllParticipants,  // 所有已注册参训玩家
	SpecificRoles,    // 指定身份的玩家
};

/**
 * 面板类任务基类：
 * 负责把 FTaskPanelData 下发到每个玩家 PlayerState 上的通道组件，
 * 并接收按钮回传。具体任务（信息展示 / 等待确认 / 身份选择）只需重写
 * BuildPanelData 和 HandlePanelAction。
 *
 * 本类不包含任何平台判断，也不创建 Widget：显示方式由外层框架 UI 决定。
 */
UCLASS(Abstract, CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_PanelBase : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void EndTask_Implementation() override;

	// ==================== 面板配置 ====================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	EUIPanelType PanelType = EUIPanelType::Custom;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FText PanelTitle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FText PanelContent;

	// 静态数据行（动态内容可在 BuildPanelData 里追加）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	TArray<FPanelRowData> PanelRows;

	// 静态按钮（动态按钮可在 BuildPanelData 里追加）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	TArray<FTaskPanelButton> PanelButtons;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	EPanelAudience Audience = EPanelAudience::AllParticipants;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel",
		meta = (EditCondition = "Audience == EPanelAudience::SpecificRoles", EditConditionHides))
	TArray<FName> TargetRoleTags;

	// 任务结束时自动关闭面板
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	bool bHidePanelOnEnd = true;

	// ==================== 面板接口 ====================

	/** 当前配置对应的接收玩家列表 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Panel")
	TArray<APlayerController*> ResolvePanelAudience() const;

	/** 向所有接收玩家下发当前面板 */
	UFUNCTION(BlueprintCallable, Category = "Panel")
	void PushPanelToAudience();

	/** 向单个玩家下发当前面板 */
	UFUNCTION(BlueprintCallable, Category = "Panel")
	void PushPanelToController(APlayerController* PC);

	/** 向单个玩家下发自定义面板（比如只对该玩家显示的提示） */
	void PushPanelDataToController(APlayerController* PC, const FTaskPanelData& Data);

	/** 关闭所有接收玩家的面板 */
	UFUNCTION(BlueprintCallable, Category = "Panel")
	void HidePanelFromAudience();

	/** 组装面板数据（子类重写以填充动态内容） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Panel")
	virtual FTaskPanelData BuildPanelData() const;

	/** 面板按钮回传处理（子类重写） */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Panel")
	void HandlePanelAction(APlayerController* PC, FName ButtonId, int32 RowIndex);
	virtual void HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex);

protected:
	/** 通道组件按钮回传的统一入口 */
	UFUNCTION()
	void HandleChannelAction(APlayerController* PC, FName PanelId, FName ButtonId, int32 RowIndex);

	void BindChannel(UTaskPanelChannelComponent* Channel);
	void UnbindAllChannels();

	// 本次任务实例的面板 Id（用对象名，实例间天然唯一）
	FName PanelInstanceId;

	// 已绑定的通道（弱引用，玩家离开时不会阻止回收）
	TArray<TWeakObjectPtr<UTaskPanelChannelComponent>> BoundChannels;
};
