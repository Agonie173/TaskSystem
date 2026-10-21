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
 *
 * 职责：警戒/待命阶段的"不得擅离"约束，本体只是把基类 ZoneMonitor 配置成
 *       "必须在区域内"（bRequireInsideZone = true）的预设。
 * 口径（以代码为准）：离开区域只弹警告面板并记一次异常（错误码 LeaveArea，
 *       经 ReportScoreEvent 按身份上报评分组件扣分），同一玩家反复进出只记
 *       一次分；回到区域即恢复正常面板并继续等待——流程不会因此失败或暂停。
 * 设计原因：停留类任务通常由剧情推进（如支援到场）结束，默认
 *       RequiredCompliantDuration = 0 表示不自动计时结束，需要定时时在配表改。
 * 用法：场景里放置带 "SafeArea" Tag 的碰撞盒即可，无需改代码；要换区域/错误
 *       码/文案时在派生 BP 覆盖对应属性。
 * 坑：仅服务器执行，Overlap 事件由 Sequencer 从关卡 TriggerVolume 转发；
 *       进入/离开/记分日志由基类 ZoneMonitor 统一输出（违规 Warning、恢复
 *       Log、结束含违规总数），本类不再重复打点。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_StayInArea : public UMultipleTask_ZoneMonitor
{
	GENERATED_BODY()

public:
	/** 构造：写死"必须在区域内"的默认配置（SafeArea / LeaveArea / 警告文案） */
	UMultipleTask_StayInArea();
};
