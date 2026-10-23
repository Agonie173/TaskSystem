// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_PlayerListener.generated.h"

class APlayerController;
class APawn;

/**
 * 玩家监听任务：在任务启动时立即扫描当前已在场的合法玩家，
 * 并持续监听后续进场的玩家，为每位匹配到的玩家 Pawn 打上带编号的标签。
 * 监听数量达标后自动结束任务。
 *
 * 典型用法：配表 Step 0 放置此任务，配合 Sequencer 级 MinPlayersToStart=1
 * 让流程不等齐玩家直接开跑，由本任务自己负责等齐 N 人后放行到后续步骤。
 *
 * 过滤逻辑：
 * - FilterTags 数组：玩家 Pawn 必须带其中任一标签才被纳入监听（留空则放行全部）。
 *   这与 Sequencer 的 ValidPlayerTags 是两套独立配置——本任务可以监听任意标签体系
 *   （如只监听 VRPlayer），不受全局 ValidPlayerTags 影响。
 * - 去重以 PlayerState->GetPlayerId() 为凭据（Pawn 指针会因占有链变更而失效，
 *   但 PlayerId 生命周期跟随整局连接，稳定可靠）。
 *
 * 打标签逻辑：
 * - 标签由 BaseTagToAssign + 自增 Counter 拼接生成（例：BaseTag=Player → Player1, Player2, ...）。
 * - 服务器本地 Pawn->Tags.AddUnique 后，经广播通道 BroadcastModifyActorTags 同步到客户端。
 * - Pawn 是复制 Actor，RPC 的 Actor 引用由引擎跨机解析到同一实例，无需客户端二次查找。
 *
 * 双通路感知：
 * - 主动通路：绑定 Sequencer.OnPlayerRegistered 委托，RegisterParticipant 成功后立即感知新玩家。
 * - 兜底通路：0.5s 轮询一次世界所有 PlayerController（兼容未走 RegisterParticipant 的手动进场、
 *   以及 Pawn 未及时占有导致的委托 Pawn==nullptr 场景）。
 *
 * 网络：仅服务器执行；任务对象不存在于客户端。
 *
 * 坑：
 * - 若训练过程中玩家 Pawn 会被销毁重生，新 Pawn 的标签不会自动保留——
 *   需重开本任务或在 Pawn 重生时手动补发。当前实现只在 Pawn 被处理瞬间打标。
 * - FilterTags 为空时放行所有玩家（含像素流客户端/观战镜头），请按需填列。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_PlayerListener : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void EndTask_Implementation() override;

	/** 阻塞任务：等齐 N 个玩家后才放行到后续步骤 */
	virtual bool ShouldBlockStepCompletion() const override { return true; }

	// ==================== 配表字段 ====================

	/** 玩家 Pawn 必须带有的过滤标签（数组——带任意一个即匹配；留空则放行全部） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Listener")
	TArray<FName> FilterTags;

	/** 需要监听并打标的玩家数量，达到后任务结束 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Listener",
		meta = (ClampMin = "1"))
	int32 RequiredPlayerCount = 2;

	/**
	 * 要打上去的标签前缀。实际标签会自动拼接自增编号（从 1 开始）。
	 * 例：BaseTag=Player → Player1, Player2, Player3, ...
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Listener")
	FName BaseTagToAssign;

	/** 兜底轮询间隔（秒），用于捕获委托未覆盖的 Pawn 延迟占有场景 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Listener",
		meta = (ClampMin = "0.1"))
	float PollInterval = 0.5f;

private:
	/**
	 * 处理单个玩家：校验过滤 → 去重 → 生成编号标签 → 服务器本地打标 + 广播客户端 → 检查完成。
	 * Pawn 为空时跳过（等待下一次轮询或委托回调携带有效 Pawn）。
	 */
	void ProcessPlayer(APlayerController* PC, APawn* Pawn);

	/** Sequencer.OnPlayerRegistered 委托回调 */
	UFUNCTION()
	void HandlePlayerRegistered(APlayerController* PC, APawn* Pawn);

	/** 兜底轮询回调：遍历世界所有 PlayerController，检查是否有新的合法玩家 */
	UFUNCTION()
	void PollTick();

	/** 任务结束前调用：解绑委托 + 停止轮询 */
	void CleanupListeners();

	/** 判定 Pawn 是否带 FilterTags 中任一标签（FilterTags 为空时放行） */
	bool PawnMatchesFilter(const APawn* Pawn) const;

	// ==================== 运行时状态（Transient，不参与复制） ====================

	/** 已处理玩家的 PlayerId 集合（去重凭据） */
	TSet<int32> ProcessedPlayerIds;

	/** 下一个标签编号（从 1 开始） */
	int32 NextTagCounter = 1;

	/** 兜底轮询定时器句柄 */
	FTimerHandle PollTimerHandle;

	/** 是否已绑定 Sequencer.OnPlayerRegistered（EndTask 时需解绑定） */
	bool bDelegateBound = false;
};
