// Copyright Epic Games, Inc. All Rights Reserved.

// 主任务配表资产（UPrimaryTaskDataAsset）的实现文件。
// 配表为三层结构：Steps（顺序步骤）→ 每步 ParallelTasks（Instanced 任务项，并行执行）
// + ScoreRules（错误码 → 扣分值，供评分组件解析）。
// 该资产是编辑期模板：运行时必须由 Sequencer 在 ExecuteStep 中 DuplicateObject
// 实例化后使用，禁止直接执行或修改模板对象（详见头文件类注释）。

#include "MultipleTask/Data/PrimaryTaskDataAsset.h"
