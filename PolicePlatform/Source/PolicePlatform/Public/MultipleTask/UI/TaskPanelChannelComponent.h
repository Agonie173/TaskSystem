// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MultipleTask/UI/TaskPanelTypes.h"
#include "TaskPanelChannelComponent.generated.h"

class APlayerController;
class APlayerState;

/** 面板状态变化（显示 / 内容更新 / 隐藏），客户端 UI 绑定此事件渲染 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTaskPanelStateChanged, const FTaskPanelData&, Panel, bool, bVisible);

/** 玩家点了面板上的按钮（仅服务器收到） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnTaskPanelActionSubmitted,
	APlayerController*, PC, FName, PanelId, FName, ButtonId, int32, RowIndex);

/** 客户端向服务器提交自定义事件（如语音识别结果等），仅服务器收到 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTaskCustomEventSubmitted,
	APlayerController*, PC, FName, EventId, const FString&, Payload);

/** 服务器向客户端下发自定义事件（如任务状态广播），仅客户端本地 UI 收到 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTaskCustomEventNotified,
	FName, EventId, const FString&, Payload);

/**
 * 玩家面板通道：挂在 PlayerState 上（由外层框架添加，缺少时服务器会自动创建）。
 *
 * 职责只有两件事：
 *   1. 服务器把面板数据写到复制属性 → 客户端 OnRep 通知外层 UI；
 *   2. 客户端把按钮 Id 通过 Server RPC 回传服务器 → 服务器广播给任务。
 *
 * 为什么必须挂 PlayerState 而不是 GameState：
 *   UE 的 Server RPC 依赖 Actor 的 Owner 链找到对应的客户端连接（NetConnection）。
 *   PlayerState 的 Owner 最终指向该玩家的 PlayerController，RPC 能正确路由到拥有者客户端；
 *   而 GameState 没有指向具体玩家的 Owner 链，其上组件发出的 Server RPC 会被引擎
 *   静默丢弃（无报错），表现为"客户端点了按钮、服务器永远收不到"。所以本组件必须由
 *   外层框架挂到每个玩家的 PlayerState 上（或依赖 FindOrCreateChannel 服务器自动补挂）。
 *
 * PanelId 防过期点击：
 *   每份面板数据自带 PanelId，客户端回传按钮时会连同 PanelId 一起提交，服务器校验
 *   与当前显示的 PanelId 一致才广播。并行任务替换/关闭面板后，旧面板上的迟到的点击
 *   会被丢弃，避免误触发已结束的任务分支。
 *
 * OnRep 手动调用约定：
 *   服务器写入复制属性（CurrentPanel/bPanelVisible）后，专用服务器（DS）上客户端会
 *   通过复制回调 OnRep_PanelState 收到通知；但单机/监听服务器上本地玩家与服务器共用
 *   同一对象，收不到复制回调，因此服务器写完属性后必须手动调用一次 OnRep_PanelState
 *   让本地 UI 立即刷新（见 ShowPanel/HidePanel 的实现）。
 *
 * 不包含任何平台判断，也不创建 Widget。PC 端加视口、VR 端用 WidgetComponent
 * 之类的显示决策全部由外层框架的 UI 蓝图完成（读 GetPanel() + 绑 OnPanelStateChanged）。
 *
 * 注意：面板数据是“每个玩家一份”，监看端可遍历各玩家 PlayerState 上的本组件读取。
 */
UCLASS(ClassGroup = (PolicePlatform), meta = (BlueprintSpawnableComponent))
class POLICEPLATFORM_API UTaskPanelChannelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTaskPanelChannelComponent();

	// ==================== 静态获取 ====================

	/** 查找玩家身上的通道组件（不创建，客户端/服务器都可用） */
	UFUNCTION(BlueprintCallable, Category = "TaskPanel")
	static UTaskPanelChannelComponent* FindChannel(APlayerController* PC);

	/**
	 * 查找玩家身上的通道组件；服务器上找不到时自动在 PlayerState 上创建。
	 * 任务系统内部使用，外层框架无需手动挂组件也能工作。
	 */
	static UTaskPanelChannelComponent* FindOrCreateChannel(APlayerController* PC);

	// ==================== 服务器接口 ====================

	/** 显示或更新面板（仅服务器） */
	UFUNCTION(BlueprintCallable, Category = "TaskPanel")
	void ShowPanel(const FTaskPanelData& InPanel);

	/**
	 * 关闭面板（仅服务器）。
	 * PanelId 非空时，只有当前显示的就是该面板才关闭，避免并行任务互相关掉对方的面板。
	 */
	UFUNCTION(BlueprintCallable, Category = "TaskPanel")
	void HidePanel(FName PanelId = NAME_None);

	/**
	 * 服务器向该玩家下发自定义事件（任务状态广播等，如 3D 选择板状态同步）。
	 * 仅服务器调用有效；客户端收到后广播 OnCustomEventNotified。
	 */
	UFUNCTION(BlueprintCallable, Category = "TaskPanel")
	void NotifyCustomEvent(FName EventId, const FString& Payload);

	// ==================== 客户端接口 ====================

	/** 当前面板数据（客户端 UI 渲染用） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskPanel")
	const FTaskPanelData& GetPanel() const { return CurrentPanel; }

	/** 当前面板是否可见（客户端 UI 判断是否渲染） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskPanel")
	bool IsPanelVisible() const { return bPanelVisible; }

	/** 客户端 UI 点击按钮时调用；服务器上调用会直接本地处理 */
	UFUNCTION(BlueprintCallable, Category = "TaskPanel")
	void SubmitPanelButton(FName ButtonId, int32 RowIndex = -1);

	/**
	 * 客户端向服务器提交自定义事件（如语音识别结果等）。
	 * 仅客户端调用有效；服务器收到后广播 OnCustomEventSubmitted。
	 * EventId 用于任务端识别事件类型，Payload 为文本数据。
	 */
	UFUNCTION(BlueprintCallable, Category = "TaskPanel")
	void SubmitCustomEvent(FName EventId, const FString& Payload);

	/** 该通道所属的 PlayerController（服务器与本地客户端可用） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskPanel")
	APlayerController* GetOwningController() const;

	// ==================== 事件 ====================

	/** 面板显示 / 更新 / 隐藏（客户端 UI 绑定） */
	UPROPERTY(BlueprintAssignable, Category = "TaskPanel")
	FOnTaskPanelStateChanged OnPanelStateChanged;

	/** 玩家点了按钮（仅服务器会广播，任务绑定） */
	UPROPERTY(BlueprintAssignable, Category = "TaskPanel")
	FOnTaskPanelActionSubmitted OnPanelActionSubmitted;

	/** 客户端提交了自定义事件（仅服务器会广播，任务绑定） */
	UPROPERTY(BlueprintAssignable, Category = "TaskPanel")
	FOnTaskCustomEventSubmitted OnCustomEventSubmitted;

	/** 服务器下发了自定义事件（仅客户端会广播，本地 UI/WorldActor 绑定） */
	UPROPERTY(BlueprintAssignable, Category = "TaskPanel")
	FOnTaskCustomEventNotified OnCustomEventNotified;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 客户端按钮回传（Server RPC，可靠）；组件必须挂 PlayerState 才有合法 Owner 链，挂在 GameState 上会被引擎静默丢弃
	UFUNCTION(Server, Reliable)
	void Server_SubmitPanelButton(FName PanelId, FName ButtonId, int32 RowIndex);

	// 客户端自定义事件回传（Server RPC，可靠）
	UFUNCTION(Server, Reliable)
	void Server_SubmitCustomEvent(FName EventId, const FString& Payload);

	// 服务器自定义事件下发（Client RPC，可靠）；监听服务器对本地玩家调用会在本地直接执行
	UFUNCTION(Client, Reliable)
	void ClientNotifyCustomEvent(FName EventId, const FString& Payload);

	// 复制回调：向本地 UI 广播 OnPanelStateChanged；非专用服务器的服务器端写属性后需手动调用
	UFUNCTION()
	void OnRep_PanelState();

	// 服务器写入的权威面板状态（每个玩家一份）
	UPROPERTY(ReplicatedUsing = OnRep_PanelState)
	FTaskPanelData CurrentPanel;

	// 当前面板可见性（与 CurrentPanel 一起复制，服务器写入后同样依赖 OnRep 通知）
	UPROPERTY(ReplicatedUsing = OnRep_PanelState)
	bool bPanelVisible = false;

private:
	// 校验可见性/按钮 Id 后广播 OnPanelActionSubmitted（服务器本地调用与 Server RPC 落地共用）
	void SubmitPanelButtonInternal(FName ButtonId, int32 RowIndex);

	// 校验 EventId 后广播 OnCustomEventSubmitted（服务器本地调用与 Server RPC 落地共用）
	void SubmitCustomEventInternal(FName EventId, const FString& Payload);
};
