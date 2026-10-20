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
 * NPC 行为任务：
 * 控制 NPC 的完整行为序列：生成 → 移动 → 停留 → 播动画 → 播音频 → 销毁
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_NPCBehavior : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
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
	FRotator SpawnRotation = FRotator::ZeroRotator;

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

	int32 CurrentBehaviorIndex = -1;

	void ExecuteNextNode();

	UFUNCTION()
	void OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

	UFUNCTION()
	void OnPlayAudioFinished();

	UFUNCTION()
	void OnAnimPlayed();

	void AdvanceToNextNode();
};
