// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTask_ZoneMonitor.h"
#include "MultipleTask_StayInArea.generated.h"

/**
 * 区域警戒待命任务（07 / 16 / 19 前的等待段）：
 * 区域 = 关卡里带 ZoneActorTag 的碰撞盒，玩家必须留在区域内。
 * 离开即提示并记录该玩家一次异常，回到区域后恢复；
 * 任务不因离开而失败，可由持续时长或外部 FinishMonitoring 结束。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_StayInArea : public UMultipleTask_ZoneMonitor
{
	GENERATED_BODY()

public:
	UMultipleTask_StayInArea();
};
