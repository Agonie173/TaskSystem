// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask/UI/TaskPanelTypes.h"
#include "MultipleTask_PanelBase.generated.h"

class APlayerController;
class UTaskPanelChannelComponent;

/**
 * 面板类任务基类：
 * 负责把 FTaskPanelData 下发到每个玩家 PlayerState 上的通道组件，
 * 并接收按钮回传。具体任务（信息展示 / 等待确认 / 身份选择）只需重写
 * BuildPanelData 和 HandlePanelAction。
 *
 * 本类不包含任何平台判断，也不创建 Widget：显示方式由外层框架 UI 决定。
 *
 * 职责：把 FTaskPanelData 组包并经每个玩家 PlayerState 上的通道组件（UTaskPanelChannelComponent）
 *      下发到目标玩家，同时接收面板按钮回传。
 * 设计原因：显示方式完全交给外层框架 UI，本类只负责数据组包、受众解析与通道绑定/解绑，
 *          从而同时兼容 PC/VR 等不同端的表现。
 * 使用方式：子类重写 BuildPanelData 填充动态内容、HandlePanelAction 处理按钮回传；
 *          StartTask 自动生成本实例面板 Id 并按 Audience 下发，EndTask 按需隐藏面板并解绑全部通道。
 * 注意坑：仅服务器有效；PanelInstanceId 取对象名（实例间天然唯一），
 *         HandleChannelAction 会丢弃过期面板的点击与非受众玩家的回传。
 */
UCLASS(Abstract, CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_PanelBase : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	// 启动任务：生成本实例的面板 Id 并按受众下发面板（仅服务器）
	virtual void StartTask_Implementation() override;

	// 结束任务：按 bHidePanelOnEnd 隐藏面板、解绑全部通道后交由基类幂等广播（仅服务器）
	virtual void EndTask_Implementation() override;

	// ==================== 面板配置 ====================
	//UI面板类型（决定外层 UI 用哪套皮肤/布局，不影响任务数据）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	EUIPanelType PanelType = EUIPanelType::Custom;

	// 静态数据行（标题/正文/列表项等均由 UI 按 PanelType 自行解读 Label/Value；动态内容可在 BuildPanelData 里追加）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	TArray<FPanelRowData> PanelRows;

	// 静态按钮（动态按钮可在 BuildPanelData 里追加）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	TArray<FTaskPanelButton> PanelButtons;
	// 哪些玩家可以看到这个 UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	EPanelAudience Audience = EPanelAudience::AllParticipants;
	//目标角色标签
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

	// 绑定通道的按钮回传委托（去重；弱引用持有，玩家离开时不会阻止通道回收）
	void BindChannel(UTaskPanelChannelComponent* Channel);

	// 解绑全部已绑定通道并清空记录（EndTask 时调用）
	void UnbindAllChannels();

	// 本次任务实例的面板 Id（用对象名，实例间天然唯一）
	FName PanelInstanceId;

	// 已绑定的通道（弱引用，玩家离开时不会阻止回收）
	TArray<TWeakObjectPtr<UTaskPanelChannelComponent>> BoundChannels;
};
