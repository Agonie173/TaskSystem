// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CourseSpeechTaskWidgetBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

// 初始化语音任务UI
void UCourseSpeechTaskWidgetBase::InitializeCourseSpeechTaskWidget(float InTimeLimit, const TArray<FString>& InKeywords)
{
	// 保存有效的任务总时长
	SpeechTaskTimeLimit = FMath::Max(InTimeLimit, 0.1f);

	// 初始化剩余时间
	SpeechRemainingTime = SpeechTaskTimeLimit;

	// 初始化倒计时显示秒数
	SpeechRemainingSeconds = FMath::CeilToInt(SpeechRemainingTime);

	// 初始化圆形倒计时进度
	SpeechRemainingPercent = 1.0f;

	// 保存当前任务的关键词
	SpeechTaskKeywords = InKeywords;

	// 根据当前模式生成普通文本或关键词蓝色文本
	BuildSpeechPromptRichText();

	// 通知蓝图设置文本、背景和运行端样式
	OnCourseSpeechTaskWidgetInitialized_BIE();

	// 立即刷新一次倒计时显示
	OnCourseSpeechCountdownUpdated_BIE(SpeechRemainingSeconds, SpeechRemainingPercent);

	// 获取当前世界
	UWorld* CurrentWorld = GetWorld();

	// 世界无效时不能启动本地UI倒计时
	if (!IsValid(CurrentWorld))
	{
		return;
	}

	// 保存本地倒计时结束时间
	SpeechCountdownEndTime = CurrentWorld->GetTimeSeconds() + SpeechTaskTimeLimit;

	// 清理可能残留的UI刷新计时器
	CurrentWorld->GetTimerManager().ClearTimer(SpeechCountdownTimerHandle);

	// 每0.05秒刷新一次倒计时UI
	CurrentWorld->GetTimerManager().SetTimer(SpeechCountdownTimerHandle, this, &UCourseSpeechTaskWidgetBase::HandleSpeechCountdownTick, 0.05f, true);
}

// 根据任务文本和关键词生成富文本
void UCourseSpeechTaskWidgetBase::BuildSpeechPromptRichText()
{
	// 获取当前模式已经选择好的提示文本
	const FString SourceText = TaskPromptText.ToString();

	// 考核模式不显示关键词颜色
	if (!bIsTrainingMode || SourceText.IsEmpty() || SpeechTaskKeywords.Num() == 0)
	{
		SpeechPromptRichText = TaskPromptText;
		return;
	}

	// 创建与文本长度相同的关键词标记数组
	TArray<bool> KeywordCharacterMask;
	KeywordCharacterMask.Init(false, SourceText.Len());

	// 遍历任务配置中的全部关键词
	for (const FString& ConfiguredKeyword : SpeechTaskKeywords)
	{
		// 清理关键词首尾空白
		FString SafeKeyword = ConfiguredKeyword;
		SafeKeyword.TrimStartAndEndInline();

		// 忽略空关键词
		if (SafeKeyword.IsEmpty())
		{
			continue;
		}

		// 从文本开头开始搜索关键词
		int32 SearchStartIndex = 0;

		// 查找当前关键词出现的所有位置
		while (SearchStartIndex < SourceText.Len())
		{
			// 查找下一处关键词
			const int32 FoundIndex = SourceText.Find(SafeKeyword, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStartIndex);

			// 没有找到时结束当前关键词搜索
			if (FoundIndex == INDEX_NONE)
			{
				break;
			}

			// 标记关键词覆盖的字符范围
			for (int32 CharacterIndex = FoundIndex; CharacterIndex < FoundIndex + SafeKeyword.Len() && CharacterIndex < KeywordCharacterMask.Num(); ++CharacterIndex)
			{
				KeywordCharacterMask[CharacterIndex] = true;
			}

			// 从本次关键词结束位置继续搜索
			SearchStartIndex = FoundIndex + SafeKeyword.Len();
		}
	}

	// 创建最终的富文本字符串
	FString RichTextString;

	// 保存当前是否已经进入关键词标签
	bool bKeywordTagOpened = false;

	// 遍历提示文本中的每一个字符
	for (int32 CharacterIndex = 0; CharacterIndex < SourceText.Len(); ++CharacterIndex)
	{
		// 进入关键词区域时添加蓝色样式标签
		if (KeywordCharacterMask[CharacterIndex] && !bKeywordTagOpened)
		{
			RichTextString.Append(TEXT("<CourseKeyword>"));
			bKeywordTagOpened = true;
		}

		// 离开关键词区域时关闭样式标签
		if (!KeywordCharacterMask[CharacterIndex] && bKeywordTagOpened)
		{
			RichTextString.Append(TEXT("</>"));
			bKeywordTagOpened = false;
		}

		// 添加当前正文字符
		RichTextString.AppendChar(SourceText[CharacterIndex]);
	}

	// 文本结尾仍在关键词范围内时关闭标签
	if (bKeywordTagOpened)
	{
		RichTextString.Append(TEXT("</>"));
	}

	// 保存蓝图RichTextBlock需要显示的富文本
	SpeechPromptRichText = FText::FromString(RichTextString);
}

// 更新本地UI倒计时
void UCourseSpeechTaskWidgetBase::HandleSpeechCountdownTick()
{
	// 获取当前世界
	UWorld* CurrentWorld = GetWorld();

	// 世界无效时停止更新
	if (!IsValid(CurrentWorld))
	{
		return;
	}

	// 根据结束时间计算当前剩余时间
	SpeechRemainingTime = FMath::Max(SpeechCountdownEndTime - CurrentWorld->GetTimeSeconds(), 0.0f);

	// 将剩余时间转换成向上取整的显示秒数
	SpeechRemainingSeconds = FMath::CeilToInt(SpeechRemainingTime);

	// 计算圆形倒计时进度
	SpeechRemainingPercent = FMath::Clamp(SpeechRemainingTime / SpeechTaskTimeLimit, 0.0f, 1.0f);

	// 通知蓝图刷新倒计时显示
	OnCourseSpeechCountdownUpdated_BIE(SpeechRemainingSeconds, SpeechRemainingPercent);

	// 剩余时间为零时停止本地UI计时器
	if (SpeechRemainingTime <= 0.0f)
	{
		CurrentWorld->GetTimerManager().ClearTimer(SpeechCountdownTimerHandle);
	}
}

// 界面销毁时清理倒计时
void UCourseSpeechTaskWidgetBase::NativeDestruct()
{
	// 获取当前世界
	if (UWorld* CurrentWorld = GetWorld())
	{
		// 清理本地UI刷新计时器
		CurrentWorld->GetTimerManager().ClearTimer(SpeechCountdownTimerHandle);
	}

	// 执行通用任务UI的销毁逻辑
	Super::NativeDestruct();
}

