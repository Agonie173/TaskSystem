// Copyright Epic Games, Inc. All Rights Reserved.

// PolicePlatform 模块入口：注册主游戏模块。
// 模块定位为纯逻辑任务系统插件（详见 PolicePlatform.h 文件头注释），
// 不 include 外层框架代码，任务流程/评分/面板均通过组件与委托对外提供。

#include "PolicePlatform.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, PolicePlatform, "PolicePlatform" );
