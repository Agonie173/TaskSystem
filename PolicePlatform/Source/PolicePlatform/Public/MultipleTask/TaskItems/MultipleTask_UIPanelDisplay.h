// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTask_PanelBase.h"
#include "MultipleTask_UIPanelDisplay.generated.h"

/**
 * 纯UI展示面板任务：
 * 显示事件情况、伤情、装备清点结果等纯信息（可带数据行/按钮）
 * 可选自动关闭延迟
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_UIPanelDisplay : public UMultipleTask_PanelBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void EndTask_Implementation() override;

	// 自动关闭延迟（0=不自动关闭，由 Sequencer/外部手动 Stop）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	float AutoCloseDelay = 0.f;

private:
	FTimerHandle AutoCloseTimerHandle;
};
