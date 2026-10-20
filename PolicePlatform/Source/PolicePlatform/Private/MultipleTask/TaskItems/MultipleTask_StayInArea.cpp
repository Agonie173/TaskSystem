// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_StayInArea.h"

UMultipleTask_StayInArea::UMultipleTask_StayInArea()
{
	// 全部玩家必须留在警戒区域内
	bRequireInsideZone = true;

	ZoneActorTag = FName(TEXT("SafeArea"));
	ViolationErrorCode = FName(TEXT("LeaveArea"));

	PanelType = EUIPanelType::ScenePrompt;
	PanelTitle = FText::FromString(TEXT("现场警戒"));
	PanelContent = FText::FromString(TEXT("请保持在警戒区域内，注意观察现场情况。"));

	WarningPanelType = EUIPanelType::ScenePrompt;
	WarningTitle = FText::FromString(TEXT("离开警戒区域"));
	WarningContent = FText::FromString(TEXT("请立即返回警戒区域，保持现场警戒。"));

	// 默认由外部结束（例如支援到场）；需要固定时长时在配表里改
	RequiredCompliantDuration = 0.f;
}
