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
	// 事件情况说明
	EventInfo,
	// 身份选择
	RoleSelect,
	// 人员伤情
	InjuryReport,
	// 装备清点
	EquipmentCheck,
	// 画面提示
	ScenePrompt,
	// 自定义
	Custom,
};

/** 面板/世界空间提示的受众范围：发给谁 */
UENUM(BlueprintType)
enum class EPanelAudience : uint8
{
	// 所有已注册参训玩家（自动剔除不带合法玩家标签的 Pawn，如像素流客户端）
	AllParticipants,
	// 指定身份的玩家
	SpecificRoles,
};

/** 面板中的一行数据（伤情列表、装备清单、身份占用情况等） */
USTRUCT(BlueprintType)
struct FPanelRowData
{
	GENERATED_BODY()
	//标签
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	FText Label;
	//值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	FText Value;
	//值颜色
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
	//标签
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
 * 标题/内容由外层 UI 根据 PanelType + Rows/Buttons 自行渲染。
 */
USTRUCT(BlueprintType)
struct FTaskPanelData
{
	GENERATED_BODY()

	// 面板实例 Id，用于丢弃过期面板上的点击
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	FName PanelId;
	//面板类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	EUIPanelType PanelType = EUIPanelType::Custom;
	//面板行数据（标题/正文/列表项等均由 UI 按 PanelType 自行解读 Row.Label/Row.Value）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	TArray<FPanelRowData> Rows;
	//任务面板按钮
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	TArray<FTaskPanelButton> Buttons;

	// 是否允许用户手动关闭
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	bool bClosable = false;
};
