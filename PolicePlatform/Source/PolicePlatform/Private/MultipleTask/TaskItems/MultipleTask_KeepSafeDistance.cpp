// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_KeepSafeDistance.h"

UMultipleTask_KeepSafeDistance::UMultipleTask_KeepSafeDistance()
{
	// 全部玩家都必须待在危险区域之外
	bRequireInsideZone = false;

	ZoneActorTag = FName(TEXT("Suspect"));
	ViolationErrorCode = FName(TEXT("SafeDistanceViolation"));

	PanelType = EUIPanelType::ScenePrompt;
	PanelTitle = FText::FromString(TEXT("安全距离"));
	PanelContent = FText::FromString(TEXT("距离正常，请与嫌疑人保持安全距离并观察其动作。"));

	WarningPanelType = EUIPanelType::ScenePrompt;
	WarningTitle = FText::FromString(TEXT("距离过近"));
	WarningContent = FText::FromString(TEXT("距离过近，请立即退回安全区域。"));

	// 默认由外部在进入实际控制阶段时调用 FinishMonitoring
	RequiredCompliantDuration = 0.f;
}
