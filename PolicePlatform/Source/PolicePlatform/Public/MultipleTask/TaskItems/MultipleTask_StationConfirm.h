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
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_StationConfirm : public UMultipleTask_PanelBase
{
	GENERATED_BODY()

public:
	UMultipleTask_StationConfirm();

	virtual void StartTask_Implementation() override;
	virtual void OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor) override;
	virtual void OnActorOverlapEnd(AActor* OverlappedActor, AActor* OtherActor) override;
	virtual FTaskPanelData BuildPanelData() const override;
	virtual void HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex) override;

	// ==================== 站位 ====================

	/** 身份 → 站位，全部同时到位才允许确认 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stations")
	TArray<FRoleStationPair> Requirements;

	// ==================== 面板 ====================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FName ConfirmButtonId = FName(TEXT("Confirm"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FText ConfirmButtonText;

	/** 未全部到位时按钮上的禁用原因 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panel")
	FText NotReadyReason;

	/** 所有身份当前是否都已到位 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Stations")
	bool AreAllArrived() const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> CachedStations;

	// 与 Requirements 一一对应的当前到位状态
	TArray<bool> ArrivedFlags;

	bool IsPawnInStation(APawn* Pawn, AActor* Station) const;

	// 任务开始时按当前重叠情况初始化，避免玩家提前站好却被漏判
	void InitArrivalFromOverlaps();
};
