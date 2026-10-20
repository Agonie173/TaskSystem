// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_UIPanelDisplay.generated.h"

class UUserWidget;

UENUM(BlueprintType)
enum class EUIPanelType : uint8
{
	EventInfo,      // 事件情况说明
	InjuryReport,   // 人员伤情
	EquipmentCheck, // 装备清点
	ScenePrompt,    // 画面提示
	Custom,         // 自定义
};

USTRUCT(BlueprintType, Blueprintable)
struct FPanelRowData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText Label;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText Value;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor ValueColor = FLinearColor::White;
};

/**
 * 纯UI展示面板任务：
 * 显示事件情况、伤情、装备清点结果等纯信息
 * 可选自动关闭延迟
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_UIPanelDisplay : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void EndTask_Implementation() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	EUIPanelType PanelType = EUIPanelType::Custom;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FText Title;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FText Content;

	// 结构化数据行（如伤情列表、装备清点）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	TArray<FPanelRowData> PanelData;

	// 自定义 Widget 类（不填用默认）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	TSoftClassPtr<UUserWidget> UIWidgetClass;

	// 自动关闭延迟（0=不自动关闭，由 Sequencer/外部手动 Stop）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	float AutoCloseDelay = 0.f;

private:
	FTimerHandle AutoCloseTimerHandle;
};
