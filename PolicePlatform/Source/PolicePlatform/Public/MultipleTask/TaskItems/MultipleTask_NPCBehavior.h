// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
//#include "AI/AItypes.h"
#include "MultipleTask_NPCBehavior.generated.h"

namespace EPathFollowingResult
{
	enum Type : int;
}

class APawn;
class AAIController;
class UAudioComponent;
class USoundBase;
class UAnimSequenceBase;

UENUM(BlueprintType)
enum class ENPCBehaviorType : uint8
{
	MoveTo,           // 移动到目标点
	Wait,             // 原地等待（X秒）
	PlayAnim,         // 播放动画/Montage
	PlayAudio,        // 播放音频（NPC自己说台词或环境音）
	LookAt,           // 面向某个目标 Actor/位置
	SetAIState,       // 调用 NPC 的自定义 AI 状态
	Destroy,          // 销毁自己
};

USTRUCT(BlueprintType, Blueprintable)
struct FNPCBehaviorNode
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	ENPCBehaviorType NodeType = ENPCBehaviorType::Wait;

	// MoveTo 目标位置
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "NodeType == ENPCBehaviorType::MoveTo", EditConditionHides))
	FVector TargetLocation = FVector::ZeroVector;

	// MoveTo 目标点（关卡 Actor Tag）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "NodeType == ENPCBehaviorType::MoveTo", EditConditionHides))
	FName TargetLocationTag = NAME_None;

	// 接受半径
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "NodeType == ENPCBehaviorType::MoveTo", EditConditionHides))
	float AcceptanceRadius = 100.f;

	// Wait 时长
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "NodeType == ENPCBehaviorType::Wait", EditConditionHides))
	float WaitDuration = 1.f;

	// PlayAnim
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "NodeType == ENPCBehaviorType::PlayAnim", EditConditionHides))
	UAnimSequenceBase* AnimToPlay = nullptr;

	// PlayAudio
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "NodeType == ENPCBehaviorType::PlayAudio", EditConditionHides))
	USoundBase* AudioToPlay = nullptr;

	// LookAt 目标 Actor Tag
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "NodeType == ENPCBehaviorType::LookAt", EditConditionHides))
	FName LookAtTargetTag = NAME_None;

	// SetAIState
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (EditCondition = "NodeType == ENPCBehaviorType::SetAIState", EditConditionHides))
	FName AIStateName = NAME_None;

	// 本节点完成后等多久才下一个
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float DelayBeforeNext = 0.f;
};

/**
 * NPC 行为任务（NPC 走位/演出序列）：
 * 控制 NPC 的完整行为序列：生成（或接管已有 Pawn）→ 移动 → 停留 → 播动画 → 播音频 → 销毁。
 *
 * 设计原因：把 NPC 演出抽象成数据化节点序列（FNPCBehaviorNode），
 * 策划在任务资产里按需编排而无需写代码；多个 NPC 时 MoveTo 节点等全部到达才推进，
 * 单个提前到达不会跳过整段移动。
 *
 * 用法：
 *  - 生成模式：配置 NPCClass + SpawnPointTag（或直接坐标），任务开始时动态 Spawn NPCCount 个；
 *  - 接管模式：勾选 bUseExistingNPC 并配 ExistingNPCTag，按 Tag 复用关卡里已有的非玩家 Pawn。
 *  移动统一由 AAIController::MoveToLocation 驱动（直接坐标或 Tag 指向的 Actor 位置），
 *  到位（或移动失败兜底）后推进下一节点。
 *
 * 坑（bReplicates / 网络语义）：本任务只在服务器执行。
 *  - 动态 Spawn 的 Pawn 即使类配置了 bReplicates，其走位表现也依赖组件同步：
 *    只有 Character（带 CharacterMovementComponent）的位置才会复制到客户端，
 *    纯 Pawn 被 AI 控制器移动时客户端完全看不到位移；
 *  - AIController 本身不复制，行为序列的动画/音频触发也没有任何 RPC/属性同步驱动，
 *    所以整个演出在 DS 下客户端不可见（已知限制，注释如实记录）；
 *  - 若需客户端可见，应使用关卡预放置的已复制 Character，或自行扩展表现同步。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_NPCBehavior : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	/** 任务开始：按配置生成或接管 NPC，随后逐节点执行行为序列（仅服务器） */
	virtual void StartTask_Implementation() override;
	/** 任务结束：清理委托/定时器，按 bAutoDestroyOnEnd 决定是否销毁生成的 NPC */
	virtual void EndTask_Implementation() override;

	// ==================== NPC 配置 ====================

	// NPC Pawn 类
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC")
	TSoftClassPtr<APawn> NPCClass;

	// 是否接管已有 NPC（不 Spawn，按 Tag 查找）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC")
	bool bUseExistingNPC = false;

	// 接管模式下查找已有 NPC 的 Tag
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC",
		meta = (EditCondition = "bUseExistingNPC == true", EditConditionHides))
	FName ExistingNPCTag;

	// Spawn 数量（支援警察 2 人）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC")
	int32 NPCCount = 1;

	// SpawnPoint Actor Tag（优先用这个）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn",
		meta = (EditCondition = "bUseExistingNPC == false", EditConditionHides))
	FName SpawnPointTag;

	// 或直接坐标（SpawnPointTag 为空时用）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn",
		meta = (EditCondition = "bUseExistingNPC == false && SpawnPointTag == None", EditConditionHides))
	FVector SpawnLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn",
		meta = (EditCondition = "bUseExistingNPC == false && SpawnPointTag == None", EditConditionHides))
	FRotator SpawnRotation = FRotator::ZeroRotator; // 直接坐标模式下的初始朝向

	// 行为序列
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behavior")
	TArray<FNPCBehaviorNode> BehaviorSequence;

	// 行为完成后是否销毁 NPC
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behavior")
	bool bAutoDestroyOnEnd = false;

private:
	// 当前控制的 NPC（可能多个）
	UPROPERTY()
	TArray<TObjectPtr<APawn>> ControlledNPCs;

	// 当前执行到的行为节点下标（-1 = 未开始）
	int32 CurrentBehaviorIndex = -1;

	/** 当前 MoveTo 节点还没走完的 NPC 数量（多 NPC 时全部到达才推进） */
	int32 PendingMoveCount = 0;

	/** 本节点里已经派发 MoveTo 的 AI 控制器（用于解绑，弱引用防悬空） */
	UPROPERTY()
	TArray<TWeakObjectPtr<AAIController>> PendingMoveControllers;

	/** 当前节点用到的音频组件（EndTask 时解绑） */
	UPROPERTY()
	TObjectPtr<UAudioComponent> CurrentAudioComponent;

	/** 节点等待/延时定时器（成员持有，EndTask 时清理） */
	FTimerHandle NodeTimerHandle;

	/** 执行下一个行为节点（序列推进的总入口，越界即结束任务） */
	void ExecuteNextNode();

	UFUNCTION()
	void OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

	UFUNCTION()
	void OnPlayAudioFinished();

	/** 动画播完回调（预留接口，当前未绑定任何委托） */
	UFUNCTION()
	void OnAnimPlayed();

	/** 处理节点间延时（DelayBeforeNext）后推进到下一节点 */
	void AdvanceToNextNode();

	/** 解绑所有移动完成委托 */
	void UnbindMoveCompleted();

	/** 清理定时器与音频委托 */
	void CleanupNodeState();
};
