// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_VoiceRecognitionWait.generated.h"

/**
 * 语音关键词配置
 */
USTRUCT(BlueprintType, Blueprintable)
struct FVoiceKeyWord
{
	GENERATED_BODY()

	// 关键词文本
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FString Keyword;

	// 本次是否必须命中
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bRequired = true;

	// 同义词（分号分隔，如 "驻地;管控区域"）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FString Synonyms;
};

UENUM(BlueprintType)
enum class EKeywordMatchPolicy : uint8
{
	All,           // 所有 RequiredKeyWords 都要命中
	Any,           // 任意一个命中即通过
	Ratio,         // 按比例（RequiredRatio）
};

/**
 * 等待语音识别回调任务：
 * 等待外部语音识别系统回调，校验关键词是否命中
 * 同时显示参考话术字幕
 *
 * 职责：被动等待外部语音识别系统在服务器上调用 ReportRecognitionResult，
 *       按关键词策略（All/Any/Ratio）校验发言文本，命中即结束；等待期间向
 *       全员展示参考话术字幕。
 * 设计原因：识别结果由外部系统异步推送，任务只做等待与校验；匹配失败靠
 *       超时 + 重试上限兜底推进，避免流程被卡死。
 * 口径（以代码为准）：本类没有任何评分错误码字段，超时或重试耗尽只是打
 *       日志后 EndTask 放行流程，不调用 ReportScoreEvent、不扣分；需要扣分
 *       时由后续任务或外层框架负责。
 * 用法：SpeakerRoleTag 指定谁发言；RequiredKeyWords 配关键词与同义词（分号
 *       分隔）；KeywordMatchPolicy 选策略；外部系统回调时 RoleTag 必须与
 *       SpeakerRoleTag 一致，否则回执被忽略。
 * 坑：仅服务器执行；RequiredKeyWords 为空时 CheckKeywordMatch 直接返回 true
 *       （任意回执都算通过）；RecognitionTimeout <= 0 时不设超时，可能无限
 *       等待；重试计数在"匹配失败"与"超时"两条路径都会累加。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_VoiceRecognitionWait : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	/** 任务开始：重试清零、展示字幕并启动超时计时（仅服务器执行） */
	virtual void StartTask_Implementation() override;
	/** 任务结束：清超时计时器并隐藏字幕（幂等，外部也可直接调用） */
	virtual void EndTask_Implementation() override;

	// ==================== 发言人配置 ====================

	// 谁需要说话
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speaker")
	FName SpeakerRoleTag = FName(TEXT("Role_1"));

	// 对讲/喊话姿态的 Montage Tag（空=不需要姿态前置，直接说话）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speaker")
	FName SpeakerPoseTag = NAME_None;

	// ==================== 字幕与关键词 ====================

	// 参考话术字幕（显示给所有玩家看）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText SubtitleText;

	// 必须命中的关键词列表
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Keywords")
	TArray<FVoiceKeyWord> RequiredKeyWords;

	// 匹配策略
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Keywords")
	EKeywordMatchPolicy KeywordMatchPolicy = EKeywordMatchPolicy::All;

	// Ratio 模式下的比例（0.6 = 60% 以上）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Keywords",
		meta = (EditCondition = "KeywordMatchPolicy == EKeywordMatchPolicy::Ratio", EditConditionHides))
	float RequiredRatio = 0.6f;

	// ==================== 超时与重试 ====================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timeout")
	float RecognitionTimeout = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timeout")
	int32 MaxRetryCount = 3;

	// ==================== 外部语音识别回调接口 ====================

	/** 语音识别结果回调（由外部语音识别系统调用） */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void ReportRecognitionResult(FName RoleTag, const FString& RecognizedText);

private:
	// 已消耗的重试次数（匹配失败与超时两条路径都会累加）
	int32 RetryCount = 0;
	// 单轮识别的超时计时器句柄（匹配失败后重开一轮时会重置）
	FTimerHandle TimeoutTimerHandle;

	// 超时回调：计入一次重试，未达上限则重开一轮等待
	void OnTimeout();
	// 关键词匹配校验：按 All/Any/Ratio 策略判定回执文本（列表为空直接通过）
	bool CheckKeywordMatch(const FString& Text);
	// 显示参考话术字幕（实际 UI 呈现由外层框架/蓝图处理）
	void ShowSubtitle();
	// 隐藏参考话术字幕
	void HideSubtitle();
};
