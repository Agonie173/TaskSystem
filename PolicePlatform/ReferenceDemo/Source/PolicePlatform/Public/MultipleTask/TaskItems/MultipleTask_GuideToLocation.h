// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_GuideToLocation.generated.h"

class AActor;

/**
 * 指引到指定位置任务：
 * 等待带 RequiredRoleTag 的玩家进入带 StationTag 的 TriggerVolume
 * 进入 = 标记该身份到位；离开 = 取消；到位后 EndTask
 *
 * 判定口径（与 MultiStationArrival 的差异）：
 * 直接对触发事件的 OtherActor 做 ActorHasTag(RequiredRoleTag) 判断——
 * 任何带该 Tag 的 Actor 进站都算到位，不经过 Sequencer 的 FindPawnByRoleTag
 * 解析"应到 Pawn"，也不要求必须是某个特定 Pawn。
 * 适合身份 Tag 已贴在 Pawn 上、或纯 Tag 驱动的单人引导场景。
 *
 * 引导标志：bShowMarker 控制是否在站位上方生成 MarkerClass Actor
 * （高度偏移 MarkerHeightOffset）；到位或任务结束时销毁，
 * EndTask_Implementation 兜底清理，Skipped/中断结束也不会残留。
 *
 * 网络：Overlap 事件由 Sequencer（GameState）在服务器转发，
 * 判定与 Marker 的生成/销毁均服务器权威。
 *
 * 坑：StartTask 会按当前重叠补一次判定（玩家可能提前站好）；
 * 出站只有单次 OverlapEnd 事件口径，未做多碰撞组件复核（不同于 MultiStationArrival）。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_GuideToLocation : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	// 开始任务：校验 Tag 配置、缓存站位、按需生成引导标志，并按当前重叠补一次到位判定
	virtual void StartTask_Implementation() override;
	// 结束任务：兜底销毁引导标志（Skipped/中断结束也覆盖）
	virtual void EndTask_Implementation() override;
	// Sequencer 转发的进站事件：进入者带 RequiredRoleTag 即置到位标志
	virtual void OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor) override;
	// Sequencer 转发的出站事件：离开者带 RequiredRoleTag 即取消到位标志
	virtual void OnActorOverlapEnd(AActor* OverlappedActor, AActor* OtherActor) override;

	// 关卡中站位碰撞盒 Actor 的 Tag（TriggerVolume）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target")
	FName StationTag;

	// 要求进入这个站位的玩家身份 Tag（如 "Role_1"）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target")
	FName RequiredRoleTag;

	// 是否在碰撞盒位置显示引导标志（地面箭头/光环）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guide")
	bool bShowMarker = true;

	// 引导标志 Actor 类
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guide",
		meta = (EditCondition = "bShowMarker == true", EditConditionHides))
	TSoftClassPtr<AActor> MarkerClass;

	// 标志离碰撞盒顶面的高度偏移
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guide",
		meta = (EditCondition = "bShowMarker == true", EditConditionHides))
	float MarkerHeightOffset = 50.f;

private:
	// 碰撞盒是否已到位
	bool bRoleInStation = false;

	UPROPERTY()
	TObjectPtr<AActor> SpawnedMarker;

	// StartTask 时按 StationTag 解析缓存的站位碰撞盒 Actor（Overlap 判定只认它）
	UPROPERTY()
	TObjectPtr<AActor> CachedStationActor;

	// 到位即清理引导标志并结束任务
	void CheckAndComplete();
};
