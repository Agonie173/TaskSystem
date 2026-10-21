// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTask_ZoneMonitor.h"
#include "MultipleTask_KeepSafeDistance.generated.h"

/**
 * 保持安全距离任务（09 / 12 步）：
 * 区域 = 嫌疑人 Pawn 上的碰撞盒（安全距离数值直接在场景里调），
 * 玩家必须待在区域外。进入即给该玩家发警告并记录一次异常，退回后恢复；
 * 任务本身不会因为有人过近而结束。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_KeepSafeDistance : public UMultipleTask_ZoneMonitor
{
	GENERATED_BODY()

public:
	UMultipleTask_KeepSafeDistance();
};
