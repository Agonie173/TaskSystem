// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/CourseTaskWidgetBase.h"
#include "CourseSpeechTaskWidgetBase.generated.h"

/**
 * 
 */
 // 为语音关键词任务提供提示文本和倒计时数据
UCLASS(Abstract, Blueprintable)
class POLICEPLATFORM_API UCourseSpeechTaskWidgetBase : public UCourseTaskWidgetBase
{
	GENERATED_BODY()
	
public:

	// 初始化语音任务的限时、关键词和富文本
	UFUNCTION(BlueprintCallable, Category = "课件语音任务UI")
	void InitializeCourseSpeechTaskWidget(float InTimeLimit, const TArray<FString>& InKeywords);

protected:

	// 保存语音任务配置的总时长
	UPROPERTY(BlueprintReadOnly, Category = "课件语音任务UI")
	float SpeechTaskTimeLimit = 0.0f;

	// 保存当前剩余时间
	UPROPERTY(BlueprintReadOnly, Category = "课件语音任务UI")
	float SpeechRemainingTime = 0.0f;

	// 保存UI需要显示的整数秒数
	UPROPERTY(BlueprintReadOnly, Category = "课件语音任务UI")
	int32 SpeechRemainingSeconds = 0;

	// 保存圆形倒计时进度，数值从1逐渐降低到0
	UPROPERTY(BlueprintReadOnly, Category = "课件语音任务UI")
	float SpeechRemainingPercent = 1.0f;

	// 保存当前语音任务配置的关键词
	UPROPERTY(BlueprintReadOnly, Category = "课件语音任务UI")
	TArray<FString> SpeechTaskKeywords;

	// 保存加入关键词颜色标签后的富文本
	UPROPERTY(BlueprintReadOnly, Category = "课件语音任务UI")
	FText SpeechPromptRichText;

	// 通知蓝图设置文本、背景和运行端样式
	UFUNCTION(BlueprintImplementableEvent, Category = "课件语音任务UI", meta = (DisplayName = "课程语音任务UI初始化完成"))
	void OnCourseSpeechTaskWidgetInitialized_BIE();

	// 通知蓝图刷新倒计时数字和圆形进度
	UFUNCTION(BlueprintImplementableEvent, Category = "课件语音任务UI", meta = (DisplayName = "课程语音任务倒计时更新"))
	void OnCourseSpeechCountdownUpdated_BIE(int32 RemainingSeconds, float RemainingPercent);

	// 界面销毁时清理本地显示计时器
	virtual void NativeDestruct() override;

private:

	// 根据训练模式和关键词生成富文本
	void BuildSpeechPromptRichText();

	// 更新本地UI倒计时
	void HandleSpeechCountdownTick();

	// 保存本地UI倒计时结束的世界时间
	float SpeechCountdownEndTime = 0.0f;

	// 保存本地UI刷新计时器
	FTimerHandle SpeechCountdownTimerHandle;
};
