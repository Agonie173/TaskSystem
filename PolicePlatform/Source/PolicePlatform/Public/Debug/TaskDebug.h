// 任务系统调试工具头文件（仅非 Shipping 编译，Shipping 下整个文件内容被裁剪）。
//
// 组成：
//   1. FTaskDebug —— 控制台命令（Task.*）的注册入口与配表校验工具：
//      Task.Help / Task.Dump / Task.Validate [AssetPath] / Task.Skip <N> /
//      Task.Finish <ClassName|All> / Task.Restart / Task.Roles / Task.Panels /
//      Task.Scores / Task.Debug <0|1>。
//      校验结果按 Error/Warning/Info 分级，Error 表示必然卡死或崩溃的配表问题。
//   2. FTaskDebugVisualizer —— cvar Task.Debug 开关的场景 Tag/区域可视化与左上角 HUD。
//
// 使用方式：游戏模块启动时调用 FTaskDebug::RegisterConsoleCommands()（幂等，可重复调用）；
// 平时只有排查配表问题时才需要关注本文件。注意 Task.Debug 带 ECVF_Cheat 标记，
// 仅用于调试环境，不要在正式考核流程中开启。

#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "Stats/Stats.h"

#if !UE_BUILD_SHIPPING

class UWorld;
class UTaskSequencerComponent;
class UPrimaryTaskDataAsset;

/** 问题严重级别 */
enum class ETaskDebugSeverity : uint8
{
	Error,    // 必然导致流程卡死或崩溃
	Warning,  // 大概率配错，但仍能运行
	Info,     // 建议项
};

/** 一条配表问题 */
struct FTaskDebugIssue
{
	// 严重级别（Error=必然卡死/崩溃，Warning=大概率配错，Info=建议项）
	ETaskDebugSeverity Severity = ETaskDebugSeverity::Warning;

	// 问题位置（如 "Step[02] Step_02 > ParallelTasks[1]"）
	FString Location;

	// 问题描述（中文，直接输出到控制台/日志）
	FString Message;
};

/**
 * 任务系统调试工具（仅非 Shipping 编译）
 *
 * 控制台命令：
 *   Task.Help / Task.Dump / Task.Validate [AssetPath] / Task.Skip <N>
 *   Task.Finish <ClassName|All> / Task.Restart / Task.Roles / Task.Panels
 *   Task.Scores / Task.Debug <0|1>
 */
class POLICEPLATFORM_API FTaskDebug
{
public:
	/** 从 World 找到 GameState 上的 Sequencer */
	static UTaskSequencerComponent* FindSequencer(UWorld* World);

	/**
	 * 全表校验。
	 * World 为空时跳过所有「场景中查找 Actor」的规则。
	 * @return 错误（Error）数量
	 */
	static int32 ValidateAsset(UWorld* World, const UPrimaryTaskDataAsset* Asset,
		TArray<FTaskDebugIssue>& OutIssues, bool bLogToConsole = true);

	/** 只校验当前步骤正在运行的任务 */
	static int32 ValidateActiveTasks(UWorld* World, TArray<FTaskDebugIssue>& OutIssues);

	/** 把问题清单输出到日志 */
	static void PrintIssues(const TArray<FTaskDebugIssue>& Issues, const FString& Title);

	/** 注册全部控制台命令（幂等） */
	static void RegisterConsoleCommands();
};

/** 场景 Tag / 区域可视化与左上角 HUD，由 cvar Task.Debug 开关 */
class POLICEPLATFORM_API FTaskDebugVisualizer : public FTickableGameObject
{
public:
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FTaskDebugVisualizer, STATGROUP_Tickables);
	}

private:
	// 上次绘制时间（用于按帧率节流可视化绘制）
	double LastDrawTime = 0.0;
};

#endif // !UE_BUILD_SHIPPING
