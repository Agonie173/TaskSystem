// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MultipleTask/Data/PrimaryTaskDataAsset.h"
#include "TaskSequencerComponent.generated.h"

class UMultipleTaskBase;
class UPrimaryTaskDataAsset;
class APawn;
class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskStepStarted, int32, StepIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskStepCompleted, int32, StepIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllStepsCompleted);

/**
 * 任务序列驱动器：挂在 GameState 上，服务器权威
 * 负责顺序执行 FTaskStep，并行启动每个 Step 内的 TaskItems
 */
UCLASS(ClassGroup = (PolicePlatform), meta = (BlueprintSpawnableComponent))
class POLICEPLATFORM_API UTaskSequencerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTaskSequencerComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ==================== 公开接口 ====================

	/** 加载任务配表并开始执行 Step 0 */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer")
	void LoadAndStartTask(UPrimaryTaskDataAsset* InTaskAsset);

	/** 停止当前所有活跃任务 */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer")
	void StopAllTasks();

	/** 跳转到指定步骤（调试/分支跳转用） */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer")
	void SkipToStep(int32 StepIndex);

	/** 获取当前正在执行的步骤索引 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer")
	int32 GetCurrentStepIndex() const { return CurrentStepIndex; }

	/** 获取总步骤数 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer")
	int32 GetTotalStepCount() const;

	/** 当前步骤是否正在执行 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer")
	bool IsRunning() const { return bIsRunning; }

	// ==================== 事件转发接口（关卡蓝图/TriggerVolume 调用） ====================

	/** 关卡 TriggerVolume 绑定 OverlapBegin 后调用此函数，转发给当前活跃任务 */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer|Events")
	void ForwardOverlapBegin(AActor* OverlappedActor, AActor* OtherActor);

	/** 关卡 TriggerVolume 绑定 OverlapEnd 后调用此函数 */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer|Events")
	void ForwardOverlapEnd(AActor* OverlappedActor, AActor* OtherActor);

	// ==================== 辅助查询接口（TaskItem 调用） ====================

	/** 根据身份 Tag 查找对应的 Pawn */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer|Utility")
	APawn* GetPawnByRoleTag(FName RoleTag) const;

	/** 根据 Tag 查找关卡中的 Actor */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer|Utility")
	AActor* FindActorByTag(FName ActorTag) const;

	/** 获取所有玩家 Pawn */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer|Utility")
	TArray<APawn*> GetAllPlayerPawns() const;

	// ==================== 事件委托 ====================

	/** 步骤开始执行时 */
	UPROPERTY(BlueprintReadOnly, BlueprintAssignable, Category = "TaskSequencer|Events")
	FOnTaskStepStarted OnStepStarted;

	/** 步骤所有并行任务完成时 */
	UPROPERTY(BlueprintReadOnly, BlueprintAssignable, Category = "TaskSequencer|Events")
	FOnTaskStepCompleted OnStepCompleted;

	/** 所有步骤执行完毕时 */
	UPROPERTY(BlueprintReadOnly, BlueprintAssignable, Category = "TaskSequencer|Events")
	FOnAllStepsCompleted OnAllStepsCompleted;

protected:
	/** 当前加载的任务配表 */
	UPROPERTY(BlueprintReadOnly, Category = "TaskSequencer")
	TObjectPtr<UPrimaryTaskDataAsset> CurrentTaskAsset;

	/** 当前步骤索引（-1 表示未开始） */
	UPROPERTY(BlueprintReadOnly, Category = "TaskSequencer")
	int32 CurrentStepIndex = -1;

	/** 是否正在运行 */
	UPROPERTY(BlueprintReadOnly, Category = "TaskSequencer")
	bool bIsRunning = false;

	/** 当前 Step 内活跃的任务实例（Instanced 副本） */
	UPROPERTY()
	TArray<TObjectPtr<UMultipleTaskBase>> ActiveTasks;

	/** 当前 Step 已完成的任务计数 */
	UPROPERTY()
	int32 CompletedTaskCount = 0;

	// ==================== 内部实现 ====================

	/** 执行指定步骤 */
	void ExecuteStep(int32 StepIndex);

	/** 所有活跃任务都结束了吗？ */
	void CheckStepCompletion();

	/** 某个任务结束时的回调 */
	UFUNCTION()
	void HandleTaskEnd();

	/** 在关卡初始化时给玩家 Pawn 分配身份 Tag */
	void AssignRoleTagsToPlayers();
};
