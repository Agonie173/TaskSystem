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
 *
 * 判定口径（与 GuideToLocation 的差异）：
 * - 进站：OverlapBegin 时先按 RoleTag 经 Sequencer 解析出"应到 Pawn"
 *   （FindPawnByRoleTag：RoleAssignments 优先，兜底按 ActorHasTag 全局匹配），
 *   且该 Pawn 必须正是本次触发事件的 OtherActor 才置 ArrivedFlags——
 *   其他 Pawn（无身份/别的身份）进站一律不算；
 * - 出站：OverlapEnd 不能只信事件，站位可能有多个碰撞组件，
 *   离开其中一个不代表真的离开，需用 GetOverlappingActors 复核（IsPawnInStation）；
 * - StartTask 只对 Tag 做 pre-check 告警（身份可能尚未分配、Pawn 可能尚未打 Tag），
 *   真正的到站判定推迟到 Overlap 时重查，不会因预检失败而漏判。
 *
 * 网络：Overlap 事件由 Sequencer（GameState）在服务器转发，判定与 EndTask 均服务器权威。
 *
 * 用法：配置 Requirements（RoleTag+StationTag 对），站位为带对应 Tag 的 TriggerVolume。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_MultiStationArrival : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	// 开始任务：解析站位缓存、初始化到位状态；配置缺失或站位找不到时直接结束
	virtual void StartTask_Implementation() override;
	// Sequencer 转发的进站事件：匹配"应到 Pawn 进了对应站位"才置位
	virtual void OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor) override;
	// Sequencer 转发的出站事件：经站位重叠复核后才取消到位标志
	virtual void OnActorOverlapEnd(AActor* OverlappedActor, AActor* OtherActor) override;

	// 身份 → 站位 的对应关系，全部同时满足才完成
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stations")
	TArray<FRoleStationPair> Requirements;

private:
	// StartTask 时按 StationTag 解析并缓存的站位 Actor，下标与 Requirements 一一对应
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> CachedStations;

	// 与 Requirements 一一对应的当前到位状态
	TArray<bool> ArrivedFlags;

	// 复核 Pawn 是否真的与站位 Actor 处于重叠中（防多碰撞组件误判离开）
	bool IsPawnInStation(APawn* Pawn, AActor* Station) const;

	// 任务开始时按当前重叠情况初始化，避免玩家提前站好却被漏判
	void InitArrivalFromOverlaps();

	// 全部 Requirement 到位则结束任务；ArrivedFlags 未初始化齐时跳过判定
	void CheckAndComplete();
};
