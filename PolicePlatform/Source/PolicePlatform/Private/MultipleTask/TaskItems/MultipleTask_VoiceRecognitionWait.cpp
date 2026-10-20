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

	// 启动超时计时器
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
	// 隐藏字幕
}
