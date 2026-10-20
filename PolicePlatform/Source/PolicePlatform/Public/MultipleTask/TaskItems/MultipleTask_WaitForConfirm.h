// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_WaitForConfirm.generated.h"

class UUserWidget;
class APlayerController;

/**
 * UI 可见性模式
 */
UENUM(BlueprintType)
enum class EUIVisibilityMode : uint8
{
	// 每个人客户端本地弹自己的UI，互相看不到（如身份选择）
	Individual,
	// 大家一起看到的UI（Multicast让所有客户端同时弹各自本地副本）
	SharedLocal,
	// 贴在场景里的3D Widget Actor
	Shared3DWidget,
};

/**
 * 确认策略：谁需要点
 */
UENUM(BlueprintType)
enum class EConfirmPolicy : uint8
{
	AllPlayers,       // 所有活跃玩家都要确认
	AnyPlayer,        // 任意一个玩家确认即结束
	SpecificRoles,    // 指定身份的玩家确认
	FirstRole_1,      // 仅 Role_1 确认（常用在指挥/上报场景）
};

/**
 * 等待玩家确认任务：
 * 弹出UI面板 + 按钮，等待指定角色点击确认后结束
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_WaitForConfirm : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;

	// ==================== UI 配置 ====================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText PanelTitle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText PanelContent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText ConfirmButtonText;

	// UI可见性模式
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	EUIVisibilityMode UIVisibilityMode = EUIVisibilityMode::Individual;

	// 自定义Widget类（不填用默认面板）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSoftClassPtr<UUserWidget> UIWidgetClass;

	// 3D Widget 世界空间位置（Shared3DWidget 时使用）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
		meta = (EditCondition = "UIVisibilityMode == EUIVisibilityMode::Shared3DWidget", EditConditionHides))
	FVector SharedWidgetLocation = FVector::ZeroVector;

	// ==================== 确认策略 ====================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Policy")
	EConfirmPolicy ConfirmPolicy = EConfirmPolicy::AllPlayers;

	// Policy=SpecificRoles 时，指定哪些身份需要确认
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Policy",
		meta = (EditCondition = "ConfirmPolicy == EConfirmPolicy::SpecificRoles", EditConditionHides))
	TArray<FName> SpecificRoleTags;

	// ==================== 外部确认接口（Client RPC → Server） ====================

	/** 某个玩家确认了（Server RPC 调用） */
	UFUNCTION(BlueprintCallable, Category = "WaitForConfirm")
	void ReportPlayerConfirmed(FName PlayerRoleTag);

private:
	// 记录哪些玩家已确认
	TSet<FName> ConfirmedRoleTags;

	// 检查确认条件是否满足
	void CheckCompletion();

	// 广播UI弹出/关闭（Multicast）
	void BroadcastShowUI();
	void BroadcastCloseUI();
};
