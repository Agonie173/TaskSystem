// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WidgetConfirmActor.generated.h"

class UWidgetComponent;
class APlayerController;

/**
 * 客户端确认面板 Actor：本地生成、不复制，承载 WidgetComponent 让玩家点击确认按钮。
 *
 * 生命周期：
 *   服务器侧 UMultipleTask_ClickConfirm 通过 TaskAudioBroadcaster.BroadcastSpawnActorOnClients
 *   广播让目标玩家的客户端生成本 Actor（bReplicates=false，服务器无对应实例）。
 *   客户端点击按钮后通过 TaskPanelChannelComponent.SubmitCustomEvent 上报服务器，
 *   服务器判定全部目标玩家确认后广播 Destroy。
 *
 * 身份标识：
 *   TaskInstanceId = 服务器任务实例的 UObject::GetFName()，上报时原样带回让服务器
 *   Sequencer.FindTaskByInstanceId 定位到正确的 ClickConfirm 任务实例。
 *
 * Widget 配置：
 *   Widget 类、DrawSize、WidgetSpace 等全部由用户在 WidgetConfirmActor 的蓝图子类里
 *   直接给 WidgetComponent 配默认值，C++ 不做任何 UI 相关设置。
 *   Widget 蓝图按钮 OnClicked 里写一行：
 *     GetTypedOuter<UWidgetConfirmActor>()->ReportConfirmed();
 *   按钮数量/布局完全自由，Widget 蓝图零 C++ 代码。
 */
UCLASS(Blueprintable, BlueprintType)
class POLICEPLATFORM_API AWidgetConfirmActor : public AActor
{
	GENERATED_BODY()

public:
	AWidgetConfirmActor();

	// ==================== 身份（服务器广播时写入，客户端透传） ====================

	// 服务器任务实例名（= 任务 UObject::GetFName()），上报时原样带回
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
	FName TaskInstanceId;

	// 本 Actor 是给谁生成的（PlayerState->GetPlayerId()），仅用于调试
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
	int32 TargetPlayerId = INDEX_NONE;

	// ==================== UI 组件 ====================

	// WidgetComponent（构造时创建，类/DrawSize/空间等在蓝图子类配置）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* WidgetComp;

	// ==================== 按钮回调接口（Widget 蓝图 OnClicked 调这个） ====================

	/**
	 * Widget 按钮点击时调用（客户端本地）。
	 * 内部：拿本地 PlayerController → 找 PlayerState 上的 TaskPanelChannelComponent →
	 * 调 SubmitCustomEvent(FName("WidgetConfirm"), TaskInstanceId.ToString()) 走 Server RPC。
	 * 服务器端 Sequencer.ForwardCustomEvent 转发到任务实例的 OnCustomEvent。
	 */
	UFUNCTION(BlueprintCallable, Category = "Confirm")
	void ReportConfirmed();

	// ==================== 服务器广播销毁入口 ====================

	/**
	 * 收服务器销毁广播时调这个（只在客户端有本地实例时执行）。
	 * 清理 WidgetComponent 绑定后自毁。
	 */
	UFUNCTION()
	void HandleServerDestroy();
};
