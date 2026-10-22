// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_VoiceRecognitionWait.generated.h"

/** 关键词匹配策略 */
UENUM(BlueprintType)
enum class EKeywordMatchPolicy : uint8
{
	All,	// 所有必填词都命中
	Any,	// 任意一个词命中
	Ratio	// 按比例命中（RequiredRatio）
};

/** 单个关键词配置（含同义词） */
USTRUCT(BlueprintType)
struct FVoiceKeyWord
{
	GENERATED_BODY()

	/** 关键词本体 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FString Keyword;

	/** 是否必填（仅 All 策略使用；false 时只参与 Any/Ratio 统计） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bRequired = true;

	/** 同义词列表，用英文分号分隔，例如 "驻地;管控区域;警戒区" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FString Synonyms;
};

/**
 * 语音识别等待任务：总时长内等待指定身份玩家通过 SubmitCustomEvent 上报的识别文本，命中关键词即结束；超时上报 TimeoutErrorCode。
 * 仅服务器执行；客户端识别结果由 PlayerState 的 TaskPanelChannelComponent 转发。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_VoiceRecognitionWait : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void EndTask_Implementation() override;
	virtual void OnCustomEvent(APlayerController* PC, FName EventId, const FString& Payload) override;

	/** 发言者身份标签（只处理该身份玩家的识别结果） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	FName SpeakerRoleTag;

	/** 语音识别关键词列表（支持同义词/近义词） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	TArray<FVoiceKeyWord> RequiredKeyWords;

	/** 总时长限制（秒），0 = 不限时一直等待 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	float TotalDuration = 30.0f;

	/** 自定义事件 ID：外部语音识别系统通过此 ID 上报识别结果 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	FName CustomEventId = TEXT("VoiceRecognition");

	/** 提示字幕文本（可选） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	FText SubtitleText;

	/** 关键词匹配策略 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	EKeywordMatchPolicy KeywordMatchPolicy = EKeywordMatchPolicy::All;

	/** 匹配比例阈值（KeywordMatchPolicy = Ratio 时生效） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "KeywordMatchPolicy == EKeywordMatchPolicy::Ratio"))
	float RequiredRatio = 0.8f;

	/** 超时错误码（超时未命中时上报扣分） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	FName TimeoutErrorCode;

private:
	bool CheckKeywordMatch(const FString& Text);
	void OnTotalTimeout();
	void ShowSubtitle();
	void HideSubtitle();

	FTimerHandle TimeoutTimerHandle;
};
