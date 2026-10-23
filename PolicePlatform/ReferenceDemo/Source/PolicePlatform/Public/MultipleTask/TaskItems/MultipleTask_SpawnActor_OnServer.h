// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_SpawnActor_OnServer.generated.h"

class AActor;

/**
 * 服务器生成 Actor 任务：在配置的世界坐标生成一个 Actor（服务器权威，自动同步到所有客户端），
 * 本身不阻塞步骤推进（纯场景表现）。
 *
 * 生命周期：StartTask 只负责生成，任务保持运行直到步骤被推进——步骤内其他任务全部
 * 结束后 Sequencer 的清理段会调用 EndTask，此时按 bDestroyOnStepEnd 决定删除还是
 * 保留生成的 Actor（保留的交给后续流程使用或手动清理）。与 UIPanelDisplay /
 * WidgetDisplay 同属非阻塞展示类任务。
 *
 * 复制语义：生成后自动 SetReplicates(true) + SetReplicateMovement(true)——Actor 蓝图
 * 本身无需开启复制（实例级设置，不污染类默认值；同一个类可被 OnClient 任务以
 * 关闭复制的方式本地生成）。生成发生在服务器，中途加入的玩家会看到（Actor 复制有状态）。
 *
 * 注意：生成使用 AlwaysSpawn 碰撞策略（忽略重叠调整，保证出现在配置坐标）；
 * 流程重开（Task.Restart）会先按删/留规则处理旧实例再重新生成。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_SpawnActor_OnServer : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void EndTask_Implementation() override;

	/** 纯场景表现，不参与步骤推进判定 */
	virtual bool ShouldBlockStepCompletion() const override { return false; }

	// 要生成的 Actor 类（蓝图类）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn")
	TSubclassOf<AActor> ActorClass;

	// 生成位置与旋转缩放（世界坐标）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn")
	FTransform SpawnTransform = FTransform::Identity;

	// 步骤结束（EndTask）时是否删除生成物；false = 保留在场景中
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn")
	bool bDestroyOnStepEnd = true;

private:
	/** StartTask 生成的 Actor 实例（EndTask 处理后置空） */
	UPROPERTY(Transient)
	TObjectPtr<AActor> SpawnedActor;
};
