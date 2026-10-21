// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTask_PanelBase.h"
#include "MultipleTask_UIPanelDisplay.generated.h"

/**
 * 纯UI展示面板任务：
 * 显示事件情况、伤情、装备清点结果等纯信息（可带数据行/按钮）
 *
 * 职责：下发纯展示面板，不阻塞步骤完成（Sequencer 推进步骤时会先 StopAllTasks
 *      自动 Hide 本面板）。AutoCloseDelay 仅作为本地兜底，0 = 完全跟随步骤生命周期。
 * 设计原因：事件播报、伤情通报、装备清点结果等步骤只需展示不需玩家操作，
 *          不应因为面板未关闭而阻塞 Sequencer 推进。
 * 使用方式：配置 PanelType/PanelRows 等后，Sequencer 推进步骤时会自动 Hide 本面板；
 *          如需在步骤内提前关闭，设 AutoCloseDelay>0 或外部手动 EndTask。
 * 注意坑：自动关闭定时器在 StartTask 注册、EndTask 清除，EndTask 幂等可安全重复调用；
 *         面板受众由基类 Audience 决定；仅服务器有效。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_UIPanelDisplay : public UMultipleTask_PanelBase
{
	GENERATED_BODY()

public:
	// 启动任务：经基类按受众下发面板，并按 AutoCloseDelay 注册自动关闭定时器（仅服务器）
	virtual void StartTask_Implementation() override;

	// 结束任务：清除自动关闭定时器后交由基类幂等广播（仅服务器，可安全重复调用）
	virtual void EndTask_Implementation() override;

	// 纯展示面板不阻塞步骤完成——Sequencer 推进步骤时会先 StopAllTasks 自动 Hide 本面板
	virtual bool ShouldBlockStepCompletion() const override { return false; }

	// 自动关闭延迟（0=不自动关闭，由 Sequencer/外部手动 Stop）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	float AutoCloseDelay = 0.f;

private:
	// 自动关闭定时器句柄（EndTask 时清除，防止任务结束后重复触发）
	FTimerHandle AutoCloseTimerHandle;
};
