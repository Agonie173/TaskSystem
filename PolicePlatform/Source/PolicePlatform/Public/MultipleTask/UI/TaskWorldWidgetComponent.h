// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "TaskWorldWidgetComponent.generated.h"

/**
 * 任务系统专用的世界空间 Widget 组件：
 * 在原生 UWidgetComponent 基础上补齐 WidgetClass 的网络复制——
 * 服务器创建并设置 Widget 类后，客户端通过 OnRep 拿到同类并重建 Widget。
 * 生命周期完全由 UMultipleTask_WidgetDisplay 管理（创建/附着/销毁），
 * 业务代码不要手工创建本组件。
 *
 * 职责：承载步骤提示 Widget 的显示与 Widget 类跨机同步。
 * 设计原因：原生 UWidgetComponent 的 WidgetClass 不参与复制，DS 下客户端
 *          收到的组件没有 Widget 可显示；本子类用 Replicated 属性 + OnRep 补齐。
 * 使用方式：由 UMultipleTask_WidgetDisplay 在服务器上 NewObject + SetupAttachment +
 *          RegisterComponent 创建；销毁统一走 DestroyComponent。
 * 注意坑：必须挂在可复制的 Actor（如 Pawn）上才会同步到客户端；
 *         Widget 实例本身不复制，仅复制 WidgetClass 由各端自行创建。
 */
UCLASS(ClassGroup = (TaskSystem), meta = (BlueprintSpawnableComponent))
class POLICEPLATFORM_API UTaskWorldWidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UTaskWorldWidgetComponent();

	// 服务器设置要下发的 Widget 类（本地立即生效，客户端经复制后 OnRep 重建）
	void SetReplicatedWidgetClass(TSubclassOf<UUserWidget> InWidgetClass);

	// 服务器设置绘制配置（DrawSize / bDrawAtDesiredSize，本地立即生效，客户端经复制后 OnRep 应用）
	void SetReplicatedDrawConfig(FVector2D InDrawSize, bool bInDrawAtDesiredSize);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// 要在客户端重建的 Widget 类（服务器写、客户端读）
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedWidgetClass)
	TSubclassOf<UUserWidget> ReplicatedWidgetClass;

	// 要在客户端应用的绘制尺寸（原生 UWidgetComponent 的 DrawSize 不参与复制）
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedDrawConfig)
	FVector2D ReplicatedDrawSize = FVector2D(300.f, 150.f);

	// 要在客户端应用的期望尺寸绘制开关
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedDrawConfig)
	bool bReplicatedDrawAtDesiredSize = false;

	// 客户端收到 Widget 类后重建本地 Widget
	UFUNCTION()
	void OnRep_ReplicatedWidgetClass();

	// 客户端收到绘制配置后应用
	UFUNCTION()
	void OnRep_ReplicatedDrawConfig();
};
