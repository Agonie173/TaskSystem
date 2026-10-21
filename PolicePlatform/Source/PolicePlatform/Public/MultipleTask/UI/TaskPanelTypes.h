// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TaskPanelTypes.generated.h"

/**
 * 面板类型：仅用于告诉外层 UI 该用哪套皮肤/布局，
 * 任务系统本身不做任何显示方式（视口 / 3D Widget）的判断。
 */
UENUM(BlueprintType)
enum class EUIPanelType : uint8
{
	EventInfo,       // 事件情况说明
	RoleSelect,      // 身份选择
	InjuryReport,    // 人员伤情
	EquipmentCheck,  // 装备清点
	ScenePrompt,     // 画面提示
	Custom,          // 自定义
};

/** 面板中的一行数据（伤情列表、装备清单、身份占用情况等） */
USTRUCT(BlueprintType)
struct FPanelRowData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	FText Value;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	FLinearColor ValueColor = FLinearColor::White;
};

/** 面板按钮 */
USTRUCT(BlueprintType)
struct FTaskPanelButton
{
	GENERATED_BODY()

	// 回传时用于区分点了哪个按钮（客户端只回传这个 Id，不接触任务逻辑）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	FName ButtonId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	FText Label;

	// 补充说明（可作 Tooltip）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	FText Description;

	// 置灰状态（如身份已被占用），由服务器下发
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	bool bEnabled = true;

	// 置灰原因（可显示为 Tooltip）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	FText DisabledReason;
};

/**
 * 通用面板数据：服务器下发到每个玩家的通道组件，由外层框架 UI 自行决定显示方式。
 * 任务系统只负责填数据、收按钮回传，不关心 PC/VR 差异。
 */
USTRUCT(BlueprintType)
struct FTaskPanelData
{
	GENERATED_BODY()

	// 面板实例 Id，用于丢弃过期面板上的点击
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	FName PanelId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	EUIPanelType PanelType = EUIPanelType::Custom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	FText Content;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	TArray<FPanelRowData> Rows;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	TArray<FTaskPanelButton> Buttons;

	// 是否允许用户手动关闭
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	bool bClosable = false;
};
