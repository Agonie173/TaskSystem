// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTask_PanelBase.h"
#include "MultipleTask_ZoneMonitor.generated.h"

class APawn;

/**
 * 区域监控任务基类：
 * 统一处理「玩家是否在某个碰撞盒区域内」的判定、提示面板、异常记录和持续时长。
 *
 *  - bRequireInsideZone = true  → 必须留在区域内（离开即异常），用于警戒待命
 *  - bRequireInsideZone = false → 必须待在区域外（进入即异常），用于保持安全距离
 *
 * 区域边界就是关卡里带 ZoneActorTag 的碰撞盒，距离/范围数值直接在场景里调。
 * 事件来自关卡 TriggerVolume 或 Pawn 自身碰撞盒经 Sequencer 的转发。
 *
 * 已知口径差异（如实记录，暂不统一）：
 * StartTask 时的初始违规判定 InitFromCurrentOverlaps 只遍历面板受众
 * （ResolvePanelAudience），而运行期 Overlap 事件路径按全部参训者
 * （IsWatchedPawn → IsParticipant）过滤；两个集合不一致时，初始状态可能
 * 漏判部分参训者的违规，之后靠事件路径逐步修正。
 *
 * 网络语义：仅服务器执行；违规上报（ReportScoreEvent）与警告面板下发都在服务器侧完成，
 * 面板数据经 PushPanelDataToController 下发给对应客户端。
 * 子类 StayInArea / KeepSafeDistance 只配置方向与文案，判定逻辑统一在基类。
 */
UCLASS(Abstract, CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_ZoneMonitor : public UMultipleTask_PanelBase
{
	GENERATED_BODY()

public:
	/** 任务开始：复位名单、下发初始面板、按当前重叠初始化违规状态并启动合规计时（仅服务器） */
	virtual void StartTask_Implementation() override;
	/** 任务结束：清理合规计时器与违规名单（PenalizedPawns 保留到下轮 Start 才复位） */
	virtual void EndTask_Implementation() override;
	/** 进入区域事件（Sequencer 转发）：按 bRequireInsideZone 判定合规/违规 */
	virtual void OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor) override;
	/** 离开区域事件（Sequencer 转发）：先用实际重叠结果复核再判定，避免多碰撞盒误报 */
	virtual void OnActorOverlapEnd(AActor* OverlappedActor, AActor* OtherActor) override;

	// ==================== 区域 ====================

	/** 区域碰撞盒 Actor 的 Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zone")
	FName ZoneActorTag;

	/** true = 必须待在区域内；false = 必须待在区域外 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zone")
	bool bRequireInsideZone = true;

	// ==================== 异常提示 ====================

	//警告类型
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zone|Warning")
	EUIPanelType WarningPanelType = EUIPanelType::ScenePrompt;
		//警告标题
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zone|Warning")
	FText WarningTitle;
	//警告内容
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zone|Warning")
	FText WarningContent;

	/** 异常记录用的错误码（评分组件按配表扣分） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zone|Warning")
	FName ViolationErrorCode;

	// ==================== 结束条件 ====================

	/**
	 * 全部玩家持续合规达到该时长后任务自动结束。
	 * <= 0 表示不自动结束，由外部调用 FinishMonitoring。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zone", meta = (ClampMin = "0.0"))
	float RequiredCompliantDuration = 0.f;

	// ==================== 接口 ====================

	/** 外部（框架/关卡蓝图）调用，结束监控 */
	UFUNCTION(BlueprintCallable, Category = "Zone")
	void FinishMonitoring();

	/** 该玩家当前是否处于违规状态 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Zone")
	bool IsPlayerViolating(APawn* PlayerPawn) const;

	/** 该玩家本任务内是否已被记录过异常 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Zone")
	bool HasPlayerBeenPenalized(APawn* PlayerPawn) const;

protected:
	/** 违规时的提示面板 */
	FTaskPanelData BuildWarningPanelData() const;

	/** 按玩家当前状态刷新其面板（违规 → 警告，合规 → 正常） */
	void UpdatePlayerPanel(APawn* PlayerPawn);

	/** 重新评估合规计时（有人违规则清零，全部合规才重新计时） */
	void RefreshComplianceTimer();

	/** 合规时长达标回调：自动结束任务 */
	void HandleCompliantDurationReached();

	/** 任务开始时玩家可能已经处在违规位置，按当前重叠初始化 */
	void InitFromCurrentOverlaps();

	/** 取 Pawn 对应的 PlayerController（AI 托管/非玩家 Pawn 返回空） */
	APlayerController* GetControllerForPawn(APawn* PlayerPawn) const;

	/** 是否是需要监控的参训玩家（未注册参与者时单机兜底） */
	bool IsWatchedPawn(APawn* PlayerPawn) const;

	/** 记录该玩家的违规（按身份上报给评分组件，同一玩家只记一次） */
	virtual void ReportViolation(APawn* PlayerPawn);

	/** 更新玩家违规状态并联动提示/计时 */
	void SetPlayerViolating(APawn* PlayerPawn, bool bViolating);

private:
	/** 当前处于违规状态的玩家 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<APawn>> ViolatingPawns;

	/** 本任务内已记录过异常的玩家 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<APawn>> PenalizedPawns;

	/** 全员合规持续计时器（有人违规则清零重启，达 RequiredCompliantDuration 后结束任务） */
	FTimerHandle ComplianceTimerHandle;
};
