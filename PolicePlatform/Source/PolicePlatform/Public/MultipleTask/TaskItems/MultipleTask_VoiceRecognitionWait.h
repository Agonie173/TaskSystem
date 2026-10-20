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
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_VoiceRecognitionWait : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
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
	int32 RetryCount = 0;
	FTimerHandle TimeoutTimerHandle;

	void OnTimeout();
	bool CheckKeywordMatch(const FString& Text);
	void ShowSubtitle();
	void HideSubtitle();
};
