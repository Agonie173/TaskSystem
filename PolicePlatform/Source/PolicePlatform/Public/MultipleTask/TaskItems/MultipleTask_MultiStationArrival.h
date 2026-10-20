// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask/Data/PrimaryTaskDataAsset.h"
#include "MultipleTask_MultiStationArrival.generated.h"

class APawn;
class AActor;

/**
 * 多人同时到位任务：
 * 配表数组里每个元素 = 一个身份 + 一个站位。
 * 数组里所有身份“当前同时”都在各自站位里才结束；少配几个就少要求几个，
 * 配 1 个即单人到达（与 GuideToLocation 行为一致，但判定基于已分配身份）。
 *
 * 注意：这是“同时存在”判定——任何一人离开，任务在离开期间不满足条件。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_MultiStationArrival : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor) override;
	virtual void OnActorOverlapEnd(AActor* OverlappedActor, AActor* OtherActor) override;

	// 身份 → 站位 的对应关系，全部同时满足才完成
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stations")
	TArray<FRoleStationPair> Requirements;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> CachedStations;

	// 与 Requirements 一一对应的当前到位状态
	TArray<bool> ArrivedFlags;

	bool IsPawnInStation(APawn* Pawn, AActor* Station) const;

	// 任务开始时按当前重叠情况初始化，避免玩家提前站好却被漏判
	void InitArrivalFromOverlaps();

	void CheckAndComplete();
};
