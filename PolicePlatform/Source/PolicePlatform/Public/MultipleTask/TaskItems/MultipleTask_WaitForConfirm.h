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
 *
 * 职责：收集确认回执并按 ConfirmPolicy（AllPlayers/AnyPlayer/SpecificRoles/FirstRole_1）判定任务完成。
 * 设计原因：确认记录分两套维度——身份维度（ConfirmedRoleTags，服务 SpecificRoles/FirstRole_1）
 *          与连接维度（ConfirmedControllers，服务 AllPlayers/AnyPlayer），两个上报接口会互相同步，
 *          以同时支持按身份与按连接两种判定方式。
 * 使用方式：配置 ConfirmButtonText 与 ConfirmPolicy 后由 Sequencer 启动；服务器侧也可不经面板，
 *          直接调用 ReportPlayerConfirmed / ReportPlayerConfirmedByController 代为上报确认。
 * 注意坑：仅服务器有效；ConfirmButtonText 为空时面板没有确认按钮，任务可能永远无法达成；
 *         AllPlayers 判定优先依赖 Sequencer 的参与者注册（GetParticipants），未注册时退化为遍历 Pawn。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_WaitForConfirm : public UMultipleTask_PanelBase
{
	GENERATED_BODY()

public:
	// 启动任务：清空确认记录后经基类按受众下发确认面板（仅服务器）
	virtual void StartTask_Implementation() override;

	// 组装面板数据：在基类数据基础上追加确认按钮（ConfirmButtonText 为空则不追加）
	virtual FTaskPanelData BuildPanelData() const override;

	// 按钮回传入口：Confirm 按钮会转为按连接维度上报确认（仅服务器，由面板通道回调触发）
	virtual void HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex) override;

	// 确认按钮文字（为空则不生成按钮）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText ConfirmButtonText;

	// ==================== 确认策略 ====================

	// 确认策略：判定任务完成需要谁确认（全员确认/任一确认/指定身份/仅 Role_1）
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

	// 判断玩家是否已确认：先查连接维度记录，再按 Pawn 上的身份 Tag 兜底比对
	bool IsConfirmedBy(APlayerController* PC) const;

	// 检查确认条件是否满足（按 ConfirmPolicy 分支判定，满足即调用 EndTask 结束任务）
	void CheckCompletion();
};
