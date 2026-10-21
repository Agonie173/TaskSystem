// Copyright Epic Games, Inc. All Rights Reserved.

// PolicePlatform 模块主头文件。
//
// 模块定位：多人训练任务系统的"纯逻辑"模块——包含任务序列（UTaskSequencerComponent）、
// 评分（UScoreComponent）、玩家面板通道（UTaskPanelChannelComponent）、任务配表
// （UPrimaryTaskDataAsset）、预放置嫌疑人（ASuspectPawn）与调试工具（FTaskDebug）。
// 模块只依赖引擎自带模块，不 include 外层框架代码；UI 展示、平台判断等由外层框架
// 通过本模块暴露的组件接口与委托完成，保持两侧单向依赖，便于独立维护与复用。

#pragma once

#include "CoreMinimal.h"

