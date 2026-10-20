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
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_GuideToLocation : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void EndTask_Implementation() override;
	virtual void OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor) override;
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

	UPROPERTY()
	TObjectPtr<AActor> CachedStationActor;

	void CheckAndComplete();
};
