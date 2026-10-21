// Copyright Epic Games, Inc. All Rights Reserved.

// PolicePlatform 模块构建规则：
// 本模块是"纯逻辑"任务系统插件（任务序列/评分/面板通道/配表/调试工具），
// 不依赖外层框架代码；新增模块依赖时优先加入 PrivateDependencyModuleNames，
// 避免把依赖传染给引用本模块的上层模块。

using UnrealBuildTool;

public class PolicePlatform : ModuleRules
{
	public PolicePlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Core/CoreUObject/Engine：UE 基础（UObject/Actor/组件/网络复制等，必备）。
		// InputCore：输入相关基础类型（UI 输入/按键定义的默认依赖）。
		// AIModule：MultipleTask_NPCBehavior 任务用 AAIController / MoveTo 驱动 NPC 行为。
		// UMG：TaskWorldWidgetComponent / MultipleTask_WidgetDisplay 直接引用
		//      UUserWidget / UWidgetComponent（世界空间提示 Widget 及其类复制）。
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "UMG" });

		// 私有依赖：仅实现文件用到、不需要暴露给上层模块的依赖加在这里（当前为空）
		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
