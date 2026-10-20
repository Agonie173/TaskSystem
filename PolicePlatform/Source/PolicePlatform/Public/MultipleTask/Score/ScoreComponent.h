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
 * 任务只上报「谁 + 错误码」，扣分值由本组件按任务配表
 * （当前步骤的 FScoreRule）解析，任务本身不关心分值。
 * 客户端通过复制的 ReplicatedScores 刷新 HUD。
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
	UPROPERTY(ReplicatedUsing = OnRep_Scores)
	TArray<FPlayerScoreEntry> ReplicatedScores;

	UFUNCTION()
	void OnRep_Scores();

	// 服务器本地不会收到 OnRep，手动通知一次
	void NotifyScoreChanged();

	FPlayerScoreEntry* FindEntry(FName RoleTag);
	const FPlayerScoreEntry* FindEntry(FName RoleTag) const;

	// 按当前任务的配表解析错误码对应的扣分值
	int32 ResolvePenalty(FName ErrorCode) const;
};
