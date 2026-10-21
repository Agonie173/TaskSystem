// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTask_PanelBase.h"
#include "MultipleTask_RoleSelect.generated.h"

/** 可选择的身份定义 */
USTRUCT(BlueprintType)
struct FRoleDefinition
{
	GENERATED_BODY()

	// 身份 Tag（会贴到选择该身份的玩家 Pawn 上，如 "Role_1"）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	FName RoleTag;

	// 显示名称（如"1号 - 指挥"）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	FText DisplayName;

	// 职责说明
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	FText Description;
};

/**
 * 身份选择任务（04 步）：
 * 向所有参训玩家下发身份选择面板，服务器权威分配，身份不可重复占用。
 * 已被占用的按钮置灰；选择成功后同步刷新所有人的面板。
 *
 * 设计：身份真相源在 Sequencer 的 RoleAssignments（AssignRole 写入），
 * 本任务自身不保存分配结果，只负责"渲染占用情况 + 收集玩家选择 + 调 AssignRole"，
 * 因此重入或中途查询时都以 Sequencer 的实时状态为准。
 *
 * 用法：配置 Roles（可选身份列表，按钮 Id = RoleTag）与 bRequireAllParticipants；
 * 玩家点击经 PanelChannel 回传到 HandlePanelAction（服务器）。
 *
 * 网络：仅服务器执行判定与分配；面板下发 / 按钮回传 / 全员刷新均走服务器权威通道。
 *
 * 坑：
 * - 确认后即锁定：AssignRole 成功后本任务不提供释放/更换入口，
 *   占用状态会一直保持到任务结束（后续任务依赖该身份）。
 * - bRequireAllParticipants=false 时也要求"配置的身份全部被占用"才结束，
 *   否则任务一启动面板就会被关掉，玩家没机会选择（见 CheckAllAssigned）。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_RoleSelect : public UMultipleTask_PanelBase
{
	GENERATED_BODY()

public:
	// 开始任务：校验身份配置并经基类下发面板；随后立即做一次完成检查（重入时可能已全部选完）
	virtual void StartTask_Implementation() override;
	// 组装面板数据：每个身份一行占用状态 + 一个选择按钮，占用与否以 Sequencer 实时记录为准
	virtual FTaskPanelData BuildPanelData() const override;
	// 面板按钮回传（服务器）：ButtonId=RoleTag，AssignRole 成功则全员刷新并检查完成，失败只回执点击者
	virtual void HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex) override;

	// 可选身份列表
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RoleSelect")
	TArray<FRoleDefinition> Roles;

	// true=所有参训玩家都选到身份才结束；false=任意一人选到即结束
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RoleSelect")
	bool bRequireAllParticipants = true;

	// 选择已被占用的身份时，给该玩家的提示文案（留空则只刷新面板）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RoleSelect")
	FText RejectedSelectionContent;

private:
	// 按 RoleTag 在 Roles 里查找身份定义，找不到返回 nullptr（用于校验按钮回传合法性）
	const FRoleDefinition* FindRole(FName RoleTag) const;

	// 查询某身份当前占用者的玩家名（经 Sequencer 身份表），未占用返回空串
	FString GetOccupantName(FName RoleTag) const;

	// 判断该玩家是否已持有任一可选身份（占用判定经 Sequencer）
	bool HasAnyRole(APlayerController* PC) const;

	// 向所有参训玩家重发面板，同步最新的占用/置灰状态
	void RefreshAllPanels();

	// 完成检查：按 bRequireAllParticipants 与配置身份占用情况判断是否结束任务
	void CheckAllAssigned();
};
