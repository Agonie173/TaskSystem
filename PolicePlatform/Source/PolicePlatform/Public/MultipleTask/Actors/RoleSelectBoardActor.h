// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoleSelectBoardActor.generated.h"

class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBoardEventReceived, FName, EventId, const FString&, Payload);

/** 解析后的单个身份状态（给 WBP 刷新按钮用） */
USTRUCT(BlueprintType)
struct FBoardRoleState
{
	GENERATED_BODY()

	/** 身份 Tag */
	UPROPERTY(BlueprintReadOnly)
	FName Tag;

	/** 显示名 */
	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	/** 状态：Free / Occupied / Locked */
	UPROPERTY(BlueprintReadOnly)
	FString State;

	/** 占用/锁定者玩家名 */
	UPROPERTY(BlueprintReadOnly)
	FString By;
};

/**
 * 3D 选择板（客户端本地生成，不经复制）：
 * 每个客户端各生成一份，本 Actor 只是"壳"——持有 WidgetComponent 并把通道下发的
 * 自定义事件转发给 WBP，把 WBP 的点击转成通道上行事件。不含任何业务逻辑。
 *
 * 使用方式（客户端蓝图/外层框架）：
 *   1. 监听 PlayerState 通道的 OnCustomEventNotified：
 *      EventId == "Role3D_Spawn" → 本地 SpawnActor（用配表下发的类路径或自定蓝图类），
 *                                  再调 ApplyBoardPayload 设置 WBP；
 *      EventId == "Role3D_Close" → 销毁本地 Board。
 *   2. WBP 绑定 OnBoardEvent：收到 "Role3D_State" 解析 JSON 刷新按钮状态。
 *   3. WBP 按钮点击 → 调 RequestSelectRole / RequestCancelRole / RequestLockRole。
 */
UCLASS(Blueprintable)
class POLICEPLATFORM_API ARoleSelectBoard : public AActor
{
	GENERATED_BODY()

public:
	ARoleSelectBoard();

	/** 3D 显示组件（WBP 挂在这里，VR/PC 显示方式由 WBP 与外层决定） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RoleSelect")
	TObjectPtr<UWidgetComponent> WidgetComponent;

	/**
	 * 应用生成载荷（客户端调用）：从 Spawn 事件 JSON 中取出 WidgetClass 路径并设置到 WidgetComponent。
	 * 位置由 SpawnActor 时的 Transform 决定，本函数只处理 WBP 类。
	 */
	UFUNCTION(BlueprintCallable, Category = "RoleSelect")
	bool ApplyBoardPayload(const FString& SpawnPayloadJson);

	/**
	 * 一站式生成（Spawner 蓝图只需这一个节点）：解析 Spawn 事件 JSON 中的 "BoardClass"（配表下发的
	 * Actor 类路径）并生成；加载失败或未配置时使用 FallbackClass。生成后自动应用配表位置与 WBP 类。
	 */
	UFUNCTION(BlueprintCallable, Category = "RoleSelect", meta = (WorldContext = "WorldContextObject", DisplayName = "Spawn Board From Payload"))
	static ARoleSelectBoard* SpawnBoardFromPayload(UObject* WorldContextObject, const FString& SpawnPayloadJson, TSubclassOf<ARoleSelectBoard> FallbackClass);

	/**
	 * 解析 Role3D_State 事件载荷为身份状态数组（WBP 刷新按钮用，静态函数可直接调用）。
	 * 解析失败时返回空数组。
	 */
	UFUNCTION(BlueprintCallable, Category = "RoleSelect", meta = (DisplayName = "Parse State Payload"))
	static TArray<FBoardRoleState> ParseStatePayload(const FString& StatePayloadJson);

	/** 上行：请求占用某身份（转 SubmitCustomEvent "Role3D_Select"） */
	UFUNCTION(BlueprintCallable, Category = "RoleSelect")
	void RequestSelectRole(FName RoleTag);

	/** 上行：释放某身份（转 SubmitCustomEvent "Role3D_Cancel"） */
	UFUNCTION(BlueprintCallable, Category = "RoleSelect")
	void RequestCancelRole(FName RoleTag);

	/** 上行：锁定当前占用（转 SubmitCustomEvent "Role3D_Lock"） */
	UFUNCTION(BlueprintCallable, Category = "RoleSelect")
	void RequestLockRole(FName RoleTag);

	/** 通道下发事件转发（WBP 绑定此事件刷新状态） */
	UPROPERTY(BlueprintAssignable, Category = "RoleSelect")
	FOnBoardEventReceived OnBoardEvent;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TObjectPtr<class UTaskPanelChannelComponent> BoundChannel;

	UFUNCTION()
	void HandleChannelEvent(FName EventId, const FString& Payload);

	void BindChannel();
	void UnbindChannel();
};
