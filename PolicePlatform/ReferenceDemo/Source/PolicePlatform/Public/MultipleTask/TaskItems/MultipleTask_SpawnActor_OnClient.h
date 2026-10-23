// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_SpawnActor_OnClient.generated.h"

class AActor;

/**
 * 客户端本地生成 Actor 任务：在匹配玩家的客户端上本地生成一个 Actor（仅该玩家可见），
 * 本身不阻塞步骤推进（纯场景表现）。
 *
 * 目标解算（服务器）：遍历所有 PlayerController，Pawn 须同时满足
 *   1. 通过 ValidPlayerTags 校验（带 VRPlayer/PCPlayer 之一，自动排除像素流等非玩家客户端）
 *   2. 带 TargetFilterTag 标签（None = 不筛选，即所有合法玩家）
 * 匹配的所有玩家各自本地生成一份（互不可见对方实例）。
 *
 * 网络语义：经音频广播通道 Multicast 下发（客户端按 PlayerState 的 PlayerId 自判，
 * PlayerState 复制可靠，不依赖不复制且客户端不可见的 Actor Tags）。客户端生成后
 * 强制 SetReplicates(false)（纯本地表现，服务器无对应实例）。玩家中途加入不补生成
 * （瞬态表现，与音频一致）。bDestroyOnStepEnd = true 时 EndTask 广播删除。
 *
 * 生命周期：与 OnServer 版一致——任务保持运行直到步骤推进，EndTask 时按配置删/留。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_SpawnActor_OnClient : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void EndTask_Implementation() override;

	/** 纯场景表现，不参与步骤推进判定 */
	virtual bool ShouldBlockStepCompletion() const override { return false; }

	// 要生成的 Actor 类（蓝图类，客户端需能加载到该类）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn")
	TSubclassOf<AActor> ActorClass;

	// 生成位置与旋转缩放（世界坐标）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn")
	FTransform SpawnTransform = FTransform::Identity;

	// 目标筛选标签（匹配 Pawn 的 ActorTag；None = 不筛选，所有合法玩家均生成）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn")
	FName TargetFilterTag = NAME_None;

	// 步骤结束（EndTask）时是否删除各客户端的本地生成物；false = 保留
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn")
	bool bDestroyOnStepEnd = true;

private:
	/** 最近一次广播生成返回的句柄（EndTask 凭此广播删除客户端本地 Actor） */
	int32 LastSpawnHandle = INDEX_NONE;
};
