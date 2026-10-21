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
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskStepStarted, int32, StepIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskStepCompleted, int32, StepIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllStepsCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoleOccupancyChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTaskChoiceMade, FName, ChoiceId, APlayerController*, Player);

/**
 * 任务序列驱动器：挂在 GameState 上，服务器权威。
 *
 * 设计说明：
 * - 全局唯一且生命周期跟随对局，所以挂 GameState 而非 GameMode（GameMode 不会复制到客户端，
 *   客户端 HUD 需要读取步骤进度）。
 * - 按顺序执行 Steps[]；每个 Step 内的 ParallelTasks 用 DuplicateObject 实例化后并行启动，
 *   全部任务 EndTask 后自动推进下一步。实例化分两阶段：先全部创建并挂好 OnTaskEnd 回调，
 *   再统一 StartTask——防止某个任务在 StartTask 内立刻结束时计数不完整导致提前推进。
 * - 任务实例只存在于服务器；客户端只能看到三个复制属性：
 *   CurrentStepIndex（步骤进度）、bIsRunning（运行中）、ReplicatedOccupiedRoles（身份占用）。
 *
 * 使用方式：
 * - 玩家进场时 RegisterParticipant(PC)（内部会自动创建面板通道）；
 * - 开始训练 LoadAndStartTask(DataAsset)；重新开始会 ClearAllRoles + ResetScores；
 * - 关卡触发盒把 Overlap 事件转发到 ForwardOverlapBegin/End（ASuspectPawn 已内置转发，
 *   普通 TriggerVolume 需要框架/关卡蓝图接线）。
 *
 * 注意事项：
 * - 本组件不能发 Server RPC：GameState 没有 Owner 链，RPC 会被 UE 静默丢弃；
 *   客户端交互必须经 PlayerState 上的 UTaskPanelChannelComponent。
 * - StopAllTasks 先摘回调再 EndTask，防止 EndTask 广播触发 CheckStepCompletion 重入推进。
 */
UCLASS(ClassGroup = (PolicePlatform), meta = (BlueprintSpawnableComponent))
class POLICEPLATFORM_API UTaskSequencerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTaskSequencerComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ==================== 任务流程 ====================

	/**
	 * 加载任务配表并开始执行 Step 0（仅服务器）。
	 * 若此前没有任何玩家调用 RegisterParticipant，会把当时连接的所有 PlayerController
	 * 自动注册为参训玩家（简化接入兜底）；晚于此刻加入的玩家仍需框架在 PostLogin 里注册。
	 */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer")
	void LoadAndStartTask(UPrimaryTaskDataAsset* InTaskAsset);

	/** 停止当前所有活跃任务 */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer")
	void StopAllTasks();

	/** 跳转到指定步骤（调试/分支跳转用） */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer")
	void SkipToStep(int32 StepIndex);

	/**
	 * 结束当前步骤中属于指定类的所有活跃任务。
	 * 用于「由外部事件决定结束」的任务（如保持安全距离、区域警戒待命）。
	 */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer")
	int32 FinishActiveTasksByClass(TSubclassOf<UMultipleTaskBase> TaskClass);

	/**
	 * 面板上的选择结果（如结束训练时的“返回列表/重新训练”），
	 * 由任务调用，框架绑定 OnTaskChoiceMade 后按 ChoiceId 处理。
	 */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer")
	void NotifyChoiceMade(FName ChoiceId, APlayerController* PC);

	/** 获取当前正在执行的步骤索引 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer")
	int32 GetCurrentStepIndex() const { return CurrentStepIndex; }

	/** 获取总步骤数 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer")
	int32 GetTotalStepCount() const;

	/** 当前步骤是否正在执行 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer")
	bool IsRunning() const { return bIsRunning; }

	// ==================== 参训玩家注册（由外层框架在玩家进场时调用） ====================

	/**
	 * 合法参训玩家的识别标签（Pawn 上必须带其中之一才被视为真人玩家）。
	 * 用于把像素流客户端、观战镜头等非玩家 Pawn 挡在受众之外：
	 * 自动注册兜底与各任务的受众解析都按该列表过滤。
	 * 默认 VRPlayer / PCPlayer，由外层在玩家初始化时给 Pawn 打上对应标签。
	 * 列表为空时不做过滤（全部放行）。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TaskSequencer|Participants")
	TArray<FName> ValidPlayerTags = { FName(TEXT("VRPlayer")), FName(TEXT("PCPlayer")) };

	/** 该 Pawn 是否是合法参训玩家（带 ValidPlayerTags 中任一标签；列表为空时放行） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer|Participants")
	bool IsPawnValidPlayer(const APawn* Pawn) const;

	/**
	 * 注册一名参训玩家。只有注册过的玩家才会被计入
	 * “所有玩家都完成”这类判定，监看端/观战端不需要调用。
	 * 仅服务器生效。
	 */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer|Participants")
	bool RegisterParticipant(APlayerController* PC);

	/** 注销参训玩家（玩家离开时调用） */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer|Participants")
	void UnregisterParticipant(APlayerController* PC);

	/**
	 * 为参训玩家分配身份（1号/2号/3号）。
	 * 角色不可重复占用：已被占用时返回 false，由调用方决定是否允许切换。
	 * 分配成功会把 RoleTag 写到该玩家当前 Pawn 的 Tags 上（供 ActorHasTag 类判定使用）；
	 * 真相源始终是 RoleAssignments 映射。
	 */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer|Participants")
	bool AssignRole(FName RoleTag, APlayerController* PC);

	/** 该玩家是否已注册为参训玩家 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer|Participants")
	bool IsParticipant(APlayerController* PC) const;

	/** 获取所有参训玩家的 PlayerController */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer|Participants")
	TArray<APlayerController*> GetParticipants() const;

	/**
	 * 根据身份 Tag 获取 PlayerController。
	 * 查找顺序：① RoleAssignments（AssignRole 写入的真相源）；
	 * ② 兜底遍历世界中所有 PlayerController，按其当前 Pawn 上的身份 Tag 匹配
	 *   （兼容未走 RegisterParticipant/AssignRole 流程、直接给 Pawn 打 Tag 的测试关卡）。
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer|Participants")
	APlayerController* GetControllerByRoleTag(FName RoleTag) const;

	/** 该身份是否已被占用 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer|Participants")
	bool IsRoleAssigned(FName RoleTag) const;

	/** 已占用的身份列表（服务器与客户端都可用，用于 UI 置灰/显示） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer|Participants")
	TArray<FName> GetOccupiedRoles() const;

	/** 清空所有身份分配（重新开始训练时调用，仅服务器） */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer|Participants")
	void ClearAllRoles();

	// ==================== 事件转发接口（关卡蓝图/TriggerVolume 调用） ====================

	/** 关卡 TriggerVolume 绑定 OverlapBegin 后调用此函数，转发给当前活跃任务 */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer|Events")
	void ForwardOverlapBegin(AActor* OverlappedActor, AActor* OtherActor);

	/** 关卡 TriggerVolume 绑定 OverlapEnd 后调用此函数 */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer|Events")
	void ForwardOverlapEnd(AActor* OverlappedActor, AActor* OtherActor);

	/** 客户端自定义事件（如语音识别结果）经 TaskPanelChannelComponent 转发到当前活跃任务 */
	UFUNCTION(BlueprintCallable, Category = "TaskSequencer|Events")
	void ForwardCustomEvent(APlayerController* PC, FName EventId, const FString& Payload);

	// ==================== 辅助查询接口（TaskItem 调用） ====================

	/**
	 * 根据身份 Tag 查找对应的 Pawn。
	 * 查找顺序：① RoleAssignments 真相源 → 取该控制器的当前 Pawn；
	 * ② 兜底直接遍历世界所有 APawn，按 Pawn 自身 Tag 匹配——不依赖占有链，
	 *   因为测试关卡里 Pawn 可能尚未被 PlayerController 占有，或根本不走占有流程。
	 * 注意：若多个 Pawn 带同一 Tag，返回迭代顺序中的第一个。
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer|Utility")
	APawn* GetPawnByRoleTag(FName RoleTag) const;

	/** 根据 Tag 查找关卡中的 Actor */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer|Utility")
	AActor* FindActorByTag(FName ActorTag) const;

	/** 获取所有参训玩家的 Pawn（不含监看端/观战端） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer|Utility")
	TArray<APawn*> GetAllPlayerPawns() const;

	/** 根据 Pawn 反查身份 Tag（服务器侧；未分配身份返回 None） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TaskSequencer|Utility")
	FName GetRoleTagByPawn(APawn* Pawn) const;

	// ==================== 调试接口（仅供 TaskDebug 只读访问） ====================

	/** 调试用：当前步骤的活跃任务（只读） */
	const TArray<TObjectPtr<UMultipleTaskBase>>& GetActiveTasksForDebug() const { return ActiveTasks; }

	/** 调试用：当前加载的任务配表 */
	UPrimaryTaskDataAsset* GetCurrentTaskAssetForDebug() const { return CurrentTaskAsset; }

	// ==================== 评分 ====================

	/**
	 * 查找错误码对应的评分规则：先查当前步骤，再查整张配表
	 * （任务在步骤切换瞬间上报时也能找到规则）
	 */
	bool FindScoreRule(FName ErrorCode, FScoreRule& OutRule) const;

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

	/** 身份占用情况变化时（客户端 UI 绑定后刷新选择面板） */
	UPROPERTY(BlueprintReadOnly, BlueprintAssignable, Category = "TaskSequencer|Events")
	FOnRoleOccupancyChanged OnRoleOccupancyChanged;

	/**
	 * 面板选择结果（如结束训练时的“返回列表/重新训练”）。
	 * 任务系统不直接切场景/重开，只把 ChoiceId 抛给框架，由框架决定后续行为。
	 */
	UPROPERTY(BlueprintReadOnly, BlueprintAssignable, Category = "TaskSequencer|Events")
	FOnTaskChoiceMade OnTaskChoiceMade;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** 当前加载的任务配表 */
	UPROPERTY(BlueprintReadOnly, Category = "TaskSequencer")
	TObjectPtr<UPrimaryTaskDataAsset> CurrentTaskAsset;

	/** 当前步骤索引（-1 表示未开始） */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentStepIndex, BlueprintReadOnly, Category = "TaskSequencer")
	int32 CurrentStepIndex = -1;

	/** 是否正在运行 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "TaskSequencer")
	bool bIsRunning = false;

	/** 当前 Step 内活跃的任务实例（由配表中的模板 Duplicate 而来，运行时独享） */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMultipleTaskBase>> ActiveTasks;

	/** 正在批量创建/启动任务，此期间禁止推进步骤（防止迭代中重入） */
	bool bStepStarting = false;

	/** 已注册的参训玩家（服务器侧，不参与复制） */
	UPROPERTY()
	TArray<TWeakObjectPtr<APlayerController>> Participants;

	/** 身份 → 玩家映射（服务器侧，不参与复制） */
	UPROPERTY()
	TMap<FName, TWeakObjectPtr<APlayerController>> RoleAssignments;

	/** 已占用身份（复制到客户端，供 UI 置灰/显示） */
	UPROPERTY(ReplicatedUsing = OnRep_OccupiedRoles, BlueprintReadOnly, Category = "TaskSequencer|Participants")
	TArray<FName> ReplicatedOccupiedRoles;

	/** 把服务器上的 RoleAssignments 同步到复制数组 */
	void RefreshOccupiedRoles();

	UFUNCTION()
	void OnRep_OccupiedRoles();

	/** 客户端同步步骤进度：广播 OnStepStarted，让客户端 HUD 也能刷新 */
	UFUNCTION()
	void OnRep_CurrentStepIndex();

	// ==================== 内部实现 ====================

	/** 执行指定步骤 */
	void ExecuteStep(int32 StepIndex);

	/** 所有活跃任务都结束了吗？ */
	void CheckStepCompletion();

	/** 某个任务结束时的回调 */
	UFUNCTION()
	void HandleTaskEnd(UMultipleTaskBase* EndedTask);

	/** 是否拥有服务器权威 */
	bool HasAuthorityForTask() const;
};
