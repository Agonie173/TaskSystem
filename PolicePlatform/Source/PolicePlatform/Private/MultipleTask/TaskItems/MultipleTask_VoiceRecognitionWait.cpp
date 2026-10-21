// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_VoiceRecognitionWait.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

void UMultipleTask_VoiceRecognitionWait::StartTask_Implementation()
{
	RetryCount = 0;

	// 显示字幕
	ShowSubtitle();

	// 配置告警：关键词列表为空时任意回执都会被判为匹配成功
	if (RequiredKeyWords.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceRecognition] RequiredKeyWords is empty, any recognition result will pass"));
	}

	// 启动超时计时器（RecognitionTimeout <= 0 时不设超时，可能一直等待回执）
	if (RecognitionTimeout > 0.f)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			World->GetTimerManager().SetTimer(TimeoutTimerHandle, this, &UMultipleTask_VoiceRecognitionWait::OnTimeout,
				RecognitionTimeout, false);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[VoiceRecognition] Waiting for role %s to speak (retry %d/%d)"),
		*SpeakerRoleTag.ToString(), RetryCount, MaxRetryCount);
}

void UMultipleTask_VoiceRecognitionWait::EndTask_Implementation()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(TimeoutTimerHandle);
	}
	HideSubtitle();
	UE_LOG(LogTemp, Log, TEXT("[VoiceRecognition] Task ended after %d retry(ies)"), RetryCount);
	Super::EndTask_Implementation();
}

void UMultipleTask_VoiceRecognitionWait::ReportRecognitionResult(FName RoleTag, const FString& RecognizedText)
{
	// 验证发言人身份
	if (RoleTag != SpeakerRoleTag)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceRecognition] Recognition from wrong role %s (expected %s)"),
			*RoleTag.ToString(), *SpeakerRoleTag.ToString());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[VoiceRecognition] Recognition result: \"%s\""), *RecognizedText);

	if (CheckKeywordMatch(RecognizedText))
	{
		UE_LOG(LogTemp, Log, TEXT("[VoiceRecognition] All required keywords matched!"));
		EndTask();
	}
	else
	{
		// 关键词不全，重试
		RetryCount++;
		if (RetryCount >= MaxRetryCount)
		{
			UE_LOG(LogTemp, Warning, TEXT("[VoiceRecognition] Max retries exceeded (%d), task ending with failure"),
				MaxRetryCount);
			EndTask();
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[VoiceRecognition] Keyword match incomplete, retry %d/%d"), RetryCount, MaxRetryCount);
			// 重置超时计时器
			UWorld* World = GetWorld();
			if (World)
			{
				World->GetTimerManager().ClearTimer(TimeoutTimerHandle);
				World->GetTimerManager().SetTimer(TimeoutTimerHandle, this, &UMultipleTask_VoiceRecognitionWait::OnTimeout,
					RecognitionTimeout, false);
			}
		}
	}
}

void UMultipleTask_VoiceRecognitionWait::OnTimeout()
{
	UE_LOG(LogTemp, Warning, TEXT("[VoiceRecognition] Timeout, retry %d/%d"), RetryCount, MaxRetryCount);
	RetryCount++;

	if (RetryCount >= MaxRetryCount)
	{
		// 超时路径的重试耗尽：无错误码字段，仅放行流程结束任务（不扣分）
		UE_LOG(LogTemp, Log, TEXT("[VoiceRecognition] Retry limit reached after timeout, task ending"));
		EndTask();
	}
	else
	{
		// 重新设置超时
		UWorld* World = GetWorld();
		if (World)
		{
			World->GetTimerManager().SetTimer(TimeoutTimerHandle, this, &UMultipleTask_VoiceRecognitionWait::OnTimeout,
				RecognitionTimeout, false);
		}
	}
}

bool UMultipleTask_VoiceRecognitionWait::CheckKeywordMatch(const FString& Text)
{
	if (RequiredKeyWords.IsEmpty())
	{
		return true; // 没配置关键词，直接通过
	}

	// 将文本转小写做不区分大小写匹配
	FString LowerText = Text.ToLower();

	int32 MatchedCount = 0;
	int32 RequiredCount = 0;
	int32 RequiredMatchedCount = 0;

	for (const FVoiceKeyWord& KW : RequiredKeyWords)
	{
		bool bThisMatched = false;
		TArray<FString> Variants;
		Variants.Add(KW.Keyword);

		// 加入同义词
		if (!KW.Synonyms.IsEmpty())
		{
			TArray<FString> SynonymList;
			KW.Synonyms.ParseIntoArray(SynonymList, TEXT(";"));
			Variants.Append(SynonymList);
		}

		for (FString& Variant : Variants)
		{
			Variant.TrimStartAndEndInline();
			if (Variant.IsEmpty()) continue;
			if (LowerText.Contains(Variant.ToLower()))
			{
				bThisMatched = true;
				break;
			}
		}

		if (bThisMatched)
		{
			MatchedCount++;
		}
		if (KW.bRequired)
		{
			RequiredCount++;
			if (bThisMatched) RequiredMatchedCount++;
		}
	}

	// 调试日志：匹配统计，便于排查识别质量问题（每次回执只打一条）
	UE_LOG(LogTemp, Verbose, TEXT("[VoiceRecognition] Match stats: matched %d/%d, required matched %d/%d, policy %d, ratio %.2f"),
		MatchedCount, RequiredKeyWords.Num(), RequiredMatchedCount, RequiredCount,
		static_cast<int32>(KeywordMatchPolicy), RequiredRatio);

	switch (KeywordMatchPolicy)
	{
	case EKeywordMatchPolicy::All:
		// 所有 required 关键词都要命中
		return RequiredCount == 0 || RequiredMatchedCount == RequiredCount;

	case EKeywordMatchPolicy::Any:
		return MatchedCount > 0;

	case EKeywordMatchPolicy::Ratio:
		if (RequiredCount == 0) return MatchedCount > 0;
		return (float)RequiredMatchedCount / (float)RequiredCount >= RequiredRatio;
	}

	return false;
}

void UMultipleTask_VoiceRecognitionWait::ShowSubtitle()
{
	if (!SubtitleText.ToString().IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("[VoiceRecognition] Subtitle shown: %s"), *SubtitleText.ToString());
		// 实际显示字幕的逻辑由蓝图/外部 UI 系统处理
	}
}

void UMultipleTask_VoiceRecognitionWait::HideSubtitle()
{
	// 隐藏字幕（实际 UI 呈现由外层框架/蓝图处理）
	UE_LOG(LogTemp, Verbose, TEXT("[VoiceRecognition] Subtitle hidden"));
}
