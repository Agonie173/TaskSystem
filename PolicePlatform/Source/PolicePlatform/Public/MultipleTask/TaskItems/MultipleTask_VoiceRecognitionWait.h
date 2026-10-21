// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_VoiceRecognitionWait.generated.h"

/**
 * 语音识别等待任务：
 * 等待指定身份的玩家通过语音说出关键词，匹配成功则结束任务。
 *
 * 工作流程：
 * 1. 显示提示文本（字幕或UI）
 * 2. 绑定自定义事件（CustomEventId）接收外部语音识别系统的识别结果
 * 3. 在 TotalDuration 秒内可多次尝试，命中关键词即提前结束
 * 4. 超时未命中：上报 TimeoutErrorCode 扣分并结束任务
 *
 * 与旧版差异：
 * - 旧版：单轮超时 + 重试次数，每次识别失败重置计时器
 * - 新版：总时长限制，期间可无限次尝试，命中即结束；超时扣分
 *
 * 外部语音识别系统接入方式：
 * 客户端识别到语音后，通过 PlayerState 上的 TaskPanelChannelComponent
 * 调用 SubmitCustomEvent(CustomEventId, RecognizedText)，服务器转发到本任务。
 *
 * 注意：本任务只在服务器执行，依赖外部语音识别系统提供文本回执。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_VoiceRecognitionWait : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void EndTask_Implementation() override;
	virtual void OnCustomEvent(APlayerController* PC, FName EventId, const FString& Payload) override;

	// ==================== 配置 ====================

	// 发言者身份标签（只处理该身份玩家的识别结果）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	FName SpeakerRoleTag;

	// 语音识别关键词列表（支持同义词/近义词）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	TArray<FVoiceKeyWord> RequiredKeyWords;

	// 总时长限制（秒），0 = 不限时一直等待
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	float TotalDuration = 30.0f;

	// 自定义事件 ID：外部语音识别系统通过此 ID 上报识别结果
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	FName CustomEventId = TEXT("VoiceRecognition");

	// 提示字幕文本（可选）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	FText SubtitleText;

	// 关键词匹配策略
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	EKeywordMatchPolicy KeywordMatchPolicy = EKeywordMatchPolicy::All;

	// 匹配比例阈值（KeywordMatchPolicy = Ratio 时生效）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "KeywordMatchPolicy == EKeywordMatchPolicy::Ratio"))
	float RequiredRatio = 0.8f;

	// 超时错误码（超时未命中时上报扣分，配表需配置对应扣分值）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
	FName TimeoutErrorCode;

private:
	// 检查识别结果是否匹配关键词
	bool CheckKeywordMatch(const FString& Text);

	// 超时处理：扣分并结束任务
	void OnTotalTimeout();

	// 显示/隐藏提示字幕
	void ShowSubtitle();
	void HideSubtitle();

	// 总超时计时器
	FTimerHandle TimeoutTimerHandle;
};
