// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_ClickConfirm.generated.h"

class AActor;
class APlayerController;

/**
 * 点击确认任务：
 * 服务器按玩家标签筛选目标玩家，通过 TaskAudioBroadcaster 给每个目标玩家的客户端
 * 广播生成 AWidgetConfirmActor（带 WidgetComponent 的本地 Actor，不复制）。
 * 玩家点击 Widget 按钮 → WidgetConfirmActor.ReportConfirmed → TaskPanelChannelComponent.SubmitCustomEvent
 * → Sequencer.ForwardCustomEvent → 本任务 OnCustomEvent 过滤后登记。
 * 所有目标玩家都确认后 EndTask。
 *
 * 生命周期：服务器实例化 → StartTask 广播生成 → OnCustomEvent 逐个登记 → 全覆盖后 EndTask →
 * EndTask 广播销毁 → 客户端 WidgetConfirmActor.HandleServerDestroy 自毁。
 *
 * 身份过滤：WidgetConfirmActor 在客户端按自身 PlayerState.PlayerId 自判是否目标，
 * 非目标客户端不会生成 Actor（Broadcaster 按 TargetPlayerIds 过滤）。
 *
 * 上报链路：客户端 SubmitCustomEvent 走 Server RPC → 服务器 Sequencer 转发 →
 * 本任务 OnCustomEvent(PC, EventId="WidgetConfirm", Payload=TaskInstanceId.ToString())
 * 过滤 Payload 匹配自身 GetFName() 后，检查 PC 是否在 TargetPCs 集合。
 *
 * 支持场景：事件介绍后全员点"我已了解"、装备清点后任意一人点"清点完毕"等。
 * ConfirmMode 区分 AllPlayers（全员确认才结束）和 AnyPlayer（任一确认即结束）。
 */
UCLASS(CollapseCategories, Blueprintable)
class POLICEPLATFORM_API UMultipleTask_ClickConfirm : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	// 启动任务：筛目标 PC → 缓存 → 广播生成 WidgetConfirmActor
	virtual void StartTask_Implementation() override;

	// 结束任务：广播销毁所有客户端本地 Actor → 调基类幂等广播
	virtual void EndTask_Implementation() override;

	// 客户端确认事件：Sequencer.ForwardCustomEvent 转发进来
	// EventId="WidgetConfirm", Payload=TaskInstanceId.ToString()
	virtual void OnCustomEvent(APlayerController* PC, FName EventId, const FString& Payload) override;

	// ==================== 配置 ====================

	// 哪些玩家标签的 Pawn 需要生成确认面板（数组内所有标签的玩家都要确认）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	TArray<FName> TargetPlayerTags;

	// 确认模式：全员确认才结束 / 任一确认即结束
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	bool bRequireAllPlayers = true;

	// WidgetConfirmActor 生成位置（世界绝对坐标）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	FTransform WidgetSpawnTransform;

	// WidgetConfirmActor 类（必须继承自 AWidgetConfirmActor；Widget 类/DrawSize 等在该蓝图子类里配）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	TSoftClassPtr<AActor> WidgetActorClass;

	// 任务结束后是否自动销毁客户端本地 Actor（EndTask 默认 true，保留项需要手动 Destroy）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	bool bDestroyOnEnd = true;

private:
	// ============== 服务器侧运行时状态（不复制） ==============

	// 启动时缓存的目标 PC 集合（服务器端）
	UPROPERTY()
	TSet<TWeakObjectPtr<APlayerController>> TargetPCs;

	// 已确认的 PC（服务器端，逐个登记）
	TSet<TWeakObjectPtr<APlayerController>> ConfirmedPCs;

	// Broadcaster 返回的 SpawnHandle，EndTask 时凭它广播销毁
	int32 SpawnHandle = INDEX_NONE;

	// 客户端上报的 EventId 常量（WidgetConfirmActor 使用）
	static const FName WidgetConfirmEventId;

	// 服务器自判 Payload 是否匹配自身 TaskInstanceId
	bool IsPayloadSelf(const FString& Payload) const;

	// 某 PC 是否已确认（弱引用查找）
	bool IsPCConfirmed(APlayerController* PC) const;

	// 某 PC 是否在目标集合里
	bool IsPCTarget(APlayerController* PC) const;

	// 检查确认条件是否满足（按 bRequireAllPlayers 分支）
	void CheckCompletion();
};
