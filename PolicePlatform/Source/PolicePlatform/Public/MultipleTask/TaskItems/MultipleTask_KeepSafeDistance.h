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
 *
 * 职责：谈判/控制阶段约束玩家与嫌疑人保持距离，本体只是把基类 ZoneMonitor
 *       配置成"必须在区域外"（bRequireInsideZone = false）的预设。
 * 检测方式（以代码为准）：基于 Overlap 事件（嫌疑人碰撞盒经 Sequencer 转发
 *       OnActorOverlapBegin/End），不是定时距离采样；安全距离就是嫌疑人
 *       Pawn 碰撞盒的形状大小，直接在场景里调。
 * 口径：过近只发警告并记一次异常（错误码 SafeDistanceViolation，按身份上报
 *       评分组件扣分），同一玩家反复进入只记一次分，退回后恢复正常面板——
 *       流程不会因此失败或暂停。
 * 设计原因：该阶段何时解除约束由剧情决定，默认 RequiredCompliantDuration = 0
 *       表示不自动结束，由外部在进入实际控制阶段时调用 FinishMonitoring。
 * 用法：给嫌疑人 Pawn 挂 "Suspect" Tag，并保证其碰撞盒覆盖安全距离范围。
 * 坑：仅服务器执行；嫌疑人未生成或没挂 Tag 时区域找不到，监控形同虚设；
 *       进入/离开/记分日志由基类 ZoneMonitor 统一输出（违规 Warning、恢复
 *       Log、结束含违规总数），本类不再重复打点。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_KeepSafeDistance : public UMultipleTask_ZoneMonitor
{
	GENERATED_BODY()

public:
	/** 构造：写死"必须在区域外"的默认配置（Suspect / SafeDistanceViolation / 警告文案） */
	UMultipleTask_KeepSafeDistance();
};
