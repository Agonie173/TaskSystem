// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_KeepSafeDistance.h"

UMultipleTask_KeepSafeDistance::UMultipleTask_KeepSafeDistance()
{
	// 全部玩家都必须待在危险区域之外（false = 必须待在区域外，进入即违规）
	bRequireInsideZone = false;

	// 监控区域：带该 Tag 的嫌疑人 Pawn 碰撞盒；违规按身份上报评分组件用的错误码
	ZoneActorTag = FName(TEXT("Suspect"));
	ViolationErrorCode = FName(TEXT("SafeDistanceViolation"));

	// 常态提示面板：任务期间展示给受众玩家
	PanelType = EUIPanelType::ScenePrompt;

	// 违规警告面板：玩家过近时替换其常态面板，退回后自动恢复
	WarningPanelType = EUIPanelType::ScenePrompt;
	WarningTitle = FText::FromString(TEXT("距离过近"));
	WarningContent = FText::FromString(TEXT("距离过近，请立即退回安全区域。"));

	// 默认由外部在进入实际控制阶段时调用 FinishMonitoring
	RequiredCompliantDuration = 0.f;
}
