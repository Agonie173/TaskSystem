// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTask_PanelBase.h"
#include "MultipleTask/Data/PrimaryTaskDataAsset.h"
#include "MultipleTask_StationConfirm.generated.h"

class APawn;
class AActor;

/**
 * 就位确认任务（19 步）：
 * 要求配表里所有身份都站在各自站位里，未全部到位时【确认】按钮禁用；
 * 全部到位后由任意一名玩家点击确认即结束。
 * 面板实时显示每个身份的到位状态。
 *
 * 判定口径：与 MultiStationArrival 相同——进站以 Sequencer 解析的
 * FindPawnByRoleTag(RoleTag)==OtherActor 为准；出站经 GetOverlappingActors
 * 复核（IsPawnInStation），防站位多碰撞组件误判。
 *
 * 与 MultiStationArrival 的语义区别：到位只是"解锁确认按钮"，不自动结束；
 * 必须等玩家经面板回传 Confirm 按钮才 EndTask。HandlePanelAction 内
 * 仍会二次校验 AreAllArrived，防止面板状态过期或越权确认。
 *
 * 网络：Overlap 转发、面板下发与确认回传均服务器权威；
 * 到位状态变化即时 PushPanelToAudience，同步刷新所有人的面板。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_StationConfirm : public UMultipleTask_PanelBase
{
	GENERATED_BODY()

public:
	UMultipleTask_StationConfirm();

	// 开始任务：解析站位缓存并按当前重叠初始化到位状态，随后经基类下发面板
	virtual void StartTask_Implementation() override;
	// Sequencer 转发的进站事件：置位并即时刷新面板
	virtual void OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor) override;
	// Sequencer 转发的出站事件：复核后取消到位标志并即时刷新面板
	virtual void OnActorOverlapEnd(AActor* OverlappedActor, AActor* OtherActor) override;
	// 组装面板数据：每个身份一行到位状态 + 确认按钮（未全部到位时禁用）
	virtual FTaskPanelData BuildPanelData() const override;
	// 面板按钮回传（服务器）：仅确认按钮且全部到位才结束任务
	virtual void HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex) override;

	// ==================== 站位 ====================

	/** 身份 → 站位，全部同时到位才允许确认 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stations")
	TArray<FRoleStationPair> Requirements;

	// ==================== 面板 ====================

	// 确认按钮的回传 Id（HandlePanelAction 按它分发）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FName ConfirmButtonId = FName(TEXT("Confirm"));

	// 确认按钮显示文字
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FText ConfirmButtonText;

	/** 未全部到位时按钮上的禁用原因 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FText NotReadyReason;

	/** 所有身份当前是否都已到位 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Stations")
	bool AreAllArrived() const;

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
};
