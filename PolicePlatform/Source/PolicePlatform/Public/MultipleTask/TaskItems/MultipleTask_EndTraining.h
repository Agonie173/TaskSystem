// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTask_PanelBase.h"
#include "MultipleTask_EndTraining.generated.h"

/**
 * 结束训练任务（23 步）：
 * 下发结束面板，提供【返回课件列表】与【重新训练本课件】两个按钮。
 * 本任务只负责把选择结果（ButtonId）交给外层框架，不执行跳转/重开逻辑。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_EndTraining : public UMultipleTask_PanelBase
{
	GENERATED_BODY()

public:
	UMultipleTask_EndTraining();

	virtual FTaskPanelData BuildPanelData() const override;
	virtual void HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex) override;

	// ==================== 按钮配置 ====================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "End")
	FName ReturnToListButtonId = FName(TEXT("ReturnToList"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "End")
	FText ReturnToListButtonText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "End")
	FName RestartButtonId = FName(TEXT("Restart"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "End")
	FText RestartButtonText;
};
