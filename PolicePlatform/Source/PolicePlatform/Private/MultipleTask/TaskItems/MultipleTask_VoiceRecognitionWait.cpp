// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_VoiceRecognitionWait.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UMultipleTask_VoiceRecognitionWait::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	// 配置告警：关键词列表为空时任意回执都会被判为匹配成功
	if (RequiredKeyWords.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceRecognition] RequiredKeyWords is empty, any recognition result will pass"));
	}

	if (CustomEventId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceRecognition] CustomEventId is None, task will never receive recognition results"));
	}

	// 显示字幕
	ShowSubtitle();

	// 启动总超时计时器（TotalDuration <= 0 时不设超时，可能一直等待）
	if (TotalDuration > 0.f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(TimeoutTimerHandle, this, &UMultipleTask_VoiceRecognitionWait::OnTotalTimeout,
				TotalDuration, false);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[VoiceRecognition] Waiting for role %s to speak (duration=%.1fs, event=%s)"),
		*SpeakerRoleTag.ToString(), TotalDuration, *CustomEventId.ToString());
}

void UMultipleTask_VoiceRecognitionWait::EndTask_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeoutTimerHandle);
	}
	HideSubtitle();
	Super::EndTask_Implementation();
}

void UMultipleTask_VoiceRecognitionWait::OnCustomEvent(APlayerController* PC, FName EventId, const FString& Payload)
{
	// 只处理匹配的自定义事件
	if (EventId != CustomEventId)
	{
		return;
	}

	// 验证发言人身份：通过 Sequencer 查找该玩家的身份标签
	if (!SpeakerRoleTag.IsNone() && OwnerSequencer)
	{
		const FName PlayerRole = OwnerSequencer->GetRoleTagByPawn(PC ? PC->GetPawn() : nullptr);
		if (PlayerRole != SpeakerRoleTag)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[VoiceRecognition] Ignored recognition from wrong role %s (expected %s)"),
				*PlayerRole.ToString(), *SpeakerRoleTag.ToString());
			return;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[VoiceRecognition] Recognition result: \"%s\""), *Payload);

	if (CheckKeywordMatch(Payload))
	{
		UE_LOG(LogTemp, Log, TEXT("[VoiceRecognition] Keywords matched, ending task"));
		EndTask();
	}
	else
	{
		float RemainingTime = -1.f;
		if (TotalDuration > 0.f && GetWorld())
		{
			RemainingTime = FMath::Max(0.f, TotalDuration - GetWorld()->GetTimerManager().GetTimerElapsed(TimeoutTimerHandle));
		}
		UE_LOG(LogTemp, Verbose, TEXT("[VoiceRecognition] Keywords not matched, continue waiting (remaining time=%.1fs)"),
			RemainingTime);
	}
}

void UMultipleTask_VoiceRecognitionWait::OnTotalTimeout()
{
	UE_LOG(LogTemp, Warning, TEXT("[VoiceRecognition] Total timeout (%.1fs), task ending with failure"), TotalDuration);

	// 上报扣分
	if (!TimeoutErrorCode.IsNone() && OwnerSequencer)
	{
		ReportScoreEvent(SpeakerRoleTag, TimeoutErrorCode);
	}

	EndTask();
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

	// 调试日志：匹配统计，便于排查识别质量问题
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
	if (!SubtitleText.IsEmpty())
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
