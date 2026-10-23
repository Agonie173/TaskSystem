// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ScoreComponent.generated.h"

class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScoreBoardChanged);

/**
 * 单个身份的评分记录（复制到客户端供 HUD 显示）
 */
USTRUCT(BlueprintType)
struct FPlayerScoreEntry
{
	GENERATED_BODY()
	//玩家标签
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	FName RoleTag;

	// 当前得分（BaseScore - 累计扣分，最低 0）
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 FinalScore = 0;

	// 累计扣分
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 TotalDeduction = 0;

	// 异常次数
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 ViolationCount = 0;

	// 已触发的错误码（去重，用于生成评分报表）
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	TArray<FName> ViolatedErrorCodes;
};

/**
 * 评分组件：挂在 GameState 上，服务器权威。
 *
 * 设计动机：任务只上报「谁 + 错误码」，扣分值由本组件按任务配表
 * （当前步骤的 FScoreRule，错误码 → 扣分值）解析，任务本身不关心分值。
 * 这样策划改分值只需改配表，不需要动任务蓝图/代码；错误码也同时
 * 用于评分报表的归因去重。
 *
 * 使用方式：服务器上调用 ReportScoreEvent(RoleTag, ErrorCode)；
 * FindOrCreateScoreComponent 会在服务器自动补挂到 GameState，外层框架
 * 不手动挂组件也能工作。客户端通过复制的 ReplicatedScores（含排序）
 * 刷新 HUD，绑定 OnScoreBoardChanged 即可。
 *
 * 坑：解析规则依赖同一 GameState 上的 UTaskSequencerComponent 提供
 * 当前步骤的规则表，Sequencer 缺失或当前步骤未配该错误码时按 0 扣分
 * （仍会计入违规次数并打 Warning 日志）。服务器本地不会收到 OnRep，
 * 由 NotifyScoreChanged 手动触发一次广播。
 */
UCLASS(ClassGroup = (PolicePlatform), meta = (BlueprintSpawnableComponent))
class POLICEPLATFORM_API UScoreComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UScoreComponent();

	/** 从 World 找 GameState 上的评分组件（客户端/服务器都可用） */
	static UScoreComponent* FindScoreComponent(const UWorld* World);

	/** 找不到时在服务器自动创建（外层框架不挂组件也能工作） */
	static UScoreComponent* FindOrCreateScoreComponent(UWorld* World);

	/** 上报一次异常（服务器权威） */
	UFUNCTION(BlueprintCallable, Category = "Score")
	void ReportScoreEvent(FName RoleTag, FName ErrorCode);

	/** 清空所有评分记录（重新开始训练时调用） */
	UFUNCTION(BlueprintCallable, Category = "Score")
	void ResetScores();

	/** 某身份的当前得分；没有记录时返回初始分 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Score")
	int32 GetScore(FName RoleTag) const;

	/** 某身份的累计扣分 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Score")
	int32 GetTotalDeduction(FName RoleTag) const;

	/** 所有身份的评分记录（按身份 Tag 排序） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Score")
	TArray<FPlayerScoreEntry> GetAllScores() const { return ReplicatedScores; }

	/** 初始分（未扣分时的得分） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0"))
	int32 BaseScore = 100;

	/** 评分板变化（服务器本地与客户端都会触发，UI 绑定后刷新） */
	UPROPERTY(BlueprintAssignable, BlueprintReadOnly, Category = "Score")
	FOnScoreBoardChanged OnScoreBoardChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// 评分记录（服务器权威，复制到客户端；写入后按 RoleTag 排序，保证 HUD 行序稳定）
	UPROPERTY(ReplicatedUsing = OnRep_Scores)
	TArray<FPlayerScoreEntry> ReplicatedScores;

	// 复制回调：广播 OnScoreBoardChanged 让客户端 UI 刷新
	UFUNCTION()
	void OnRep_Scores();

	// 服务器本地不会收到 OnRep，手动通知一次
	void NotifyScoreChanged();

	// 按 RoleTag 查找评分记录，不存在时返回 nullptr（const 供只读查询用）
	FPlayerScoreEntry* FindEntry(FName RoleTag);
	const FPlayerScoreEntry* FindEntry(FName RoleTag) const;

	// 按当前任务的配表解析错误码对应的扣分值
	int32 ResolvePenalty(FName ErrorCode) const;
};
