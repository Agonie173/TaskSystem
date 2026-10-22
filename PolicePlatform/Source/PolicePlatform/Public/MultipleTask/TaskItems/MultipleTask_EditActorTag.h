// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_EditActorTag.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EActorTagOperation : uint8
{
	AddTag,     // 给匹配 Actor 添加标签
	RemoveTag   // 从匹配 Actor 移除标签
};

/**
 * Actor 标签修改任务：按查找标签找到场景中的 Actor，为其添加/移除指定标签。
 *
 * 目标解算：遍历世界所有带 FindTag 的 Actor（支持一个或多个）。仅处理开启复制的
 * Actor——Tags 属性不复制，两端一致修改的前提是客户端存在同一实例；非复制 Actor
 * 跳过并打警告（否则会出现服务器有标签、客户端没有的不一致状态）。
 *
 * 网络语义：服务器本地修改后经广播通道下发，RPC 的 Actor 引用参数由引擎跨机自动
 * 解析到客户端同一实例，客户端本地同步修改。RPC 到达时客户端尚未收到该 Actor
 * （如刚生成即改标签的极端时序）会打警告且该端修改丢失。
 *
 * 生命周期：一次性动作——StartTask 完成修改后立即结束（非阻塞，不影响步骤推进判定）。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_EditActorTag : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;

	/** 一次性动作，不参与步骤推进判定 */
	virtual bool ShouldBlockStepCompletion() const override { return false; }

	// 查找标签（所有带此标签的 Actor 都会被处理）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tag")
	FName FindTag = NAME_None;

	// 操作类型（添加 / 移除）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tag")
	EActorTagOperation Operation = EActorTagOperation::AddTag;

	// 要添加/移除的标签
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tag")
	FName TagToModify = NAME_None;
};
