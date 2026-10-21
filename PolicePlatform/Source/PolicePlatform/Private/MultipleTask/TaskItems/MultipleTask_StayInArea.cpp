// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_StayInArea.h"

UMultipleTask_StayInArea::UMultipleTask_StayInArea()
{
	// 全部玩家必须留在警戒区域内（true = 必须待在区域内，离开即违规）
	bRequireInsideZone = true;

	// 监控区域：场景中带该 Tag 的碰撞盒；违规按身份上报评分组件用的错误码
	ZoneActorTag = FName(TEXT("SafeArea"));
	ViolationErrorCode = FName(TEXT("LeaveArea"));

	// 常态提示面板：任务期间展示给受众玩家
	PanelType = EUIPanelType::ScenePrompt;

	// 违规警告面板：玩家离开区域时替换其常态面板，返回后自动恢复
	WarningPanelType = EUIPanelType::ScenePrompt;
	WarningTitle = FText::FromString(TEXT("离开警戒区域"));
	WarningContent = FText::FromString(TEXT("请立即返回警戒区域，保持现场警戒。"));

	// 默认由外部结束（例如支援到场）；需要固定时长时在配表里改
	RequiredCompliantDuration = 0.f;
}
