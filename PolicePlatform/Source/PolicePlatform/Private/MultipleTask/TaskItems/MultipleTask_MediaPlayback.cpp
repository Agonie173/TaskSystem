// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_MediaPlayback.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "MultipleTask/TaskSequencerComponent.h"

void UMultipleTask_MediaPlayback::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	if (Playlist.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MediaPlayback] Playlist is empty, ending immediately"));
		EndTask();
		return;
	}

	CurrentIndex = 0;
	bIsPreDelay = false;
	bAudioFinished = false;
	bAnimFinished = false;
	PlayCurrentItem();
}

void UMultipleTask_MediaPlayback::EndTask_Implementation()
{
	CleanupCurrentPlayback();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DelayTimerHandle);
		World->GetTimerManager().ClearTimer(AnimTimerHandle);
	}

	Super::EndTask_Implementation();
}

void UMultipleTask_MediaPlayback::PlayCurrentItem()
{
	if (CurrentIndex >= Playlist.Num())
	{
		// 全部播放完成
		UE_LOG(LogTemp, Log, TEXT("[MediaPlayback] Playlist finished"));
		EndTask();
		return;
	}

	const FMediaPlaybackItem& Item = Playlist[CurrentIndex];

	UE_LOG(LogTemp, Log, TEXT("[MediaPlayback] Playing item %d/%d"), CurrentIndex + 1, Playlist.Num());

	// 重置完成标志
	bAudioFinished = false;
	bAnimFinished = false;

	// 播放前延迟
	if (Item.DelayBefore > 0.f)
	{
		bIsPreDelay = true;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(DelayTimerHandle, this, &UMultipleTask_MediaPlayback::OnDelayTimerEnd,
				Item.DelayBefore, false);
		}
		return;
	}

	bIsPreDelay = false;
	PlayNext();
}

void UMultipleTask_MediaPlayback::PlayNext()
{
	const FMediaPlaybackItem& Item = Playlist[CurrentIndex];

	// 解析目标 Pawn
	CurrentTargetPawn = ResolveTargetPawn(Item);

	// 播放音频
	if (Item.Sound)
	{
		PlayAudio(Item);
	}

	// 播放动画（OnPawn 模式）
	bool bAnimPlaying = false;
	if (Item.AnimSequence && Item.LocationType == EMediaPlayLocationType::OnPawn)
	{
		if (CurrentTargetPawn)
		{
			PlayAnimOnPawn(Item.AnimSequence, CurrentTargetPawn, Item.LoopCount);
			bAnimPlaying = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[MediaPlayback] Anim sequence configured but no target Pawn found, skipping anim"));
		}
	}
	else if (Item.AnimSequence)
	{
		// 动画要求 OnPawn，但配置了 AtLocation，警告
		UE_LOG(LogTemp, Warning, TEXT("[MediaPlayback] Anim sequence requires OnPawn mode, but LocationType is AtLocation"));
	}

	// 处理完成逻辑
	if (!Item.Sound && !bAnimPlaying)
	{
		// 既没有音频也没有动画，直接推进
		UE_LOG(LogTemp, Warning, TEXT("[MediaPlayback] Item %d has neither Sound nor AnimSequence playing, skipping"), CurrentIndex);
		OnItemFinished();
		return;
	}

	// 不等待动画时，动画立即标记为完成
	if (bAnimPlaying && !Item.bWaitAnimEnd)
	{
		bAnimFinished = true;
	}

	// 如果没有动画或动画不等待，且没有音频，直接推进
	if (!Item.Sound && (!bAnimPlaying || bAnimFinished))
	{
		OnItemFinished();
	}
	// 其他情况：等音频回调和/或动画定时器
}

void UMultipleTask_MediaPlayback::PlayAudio(const FMediaPlaybackItem& Item)
{
	switch (Item.LocationType)
	{
	case EMediaPlayLocationType::AtLocation:
		PlayAudioAtLocation(Item.Sound, Item.Location);
		break;
	case EMediaPlayLocationType::OnPawn:
		if (CurrentTargetPawn)
		{
			PlayAudioOnPawn(Item.Sound, CurrentTargetPawn);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[MediaPlayback] Audio OnPawn configured but no target Pawn found, skipping audio"));
		}
		break;
	}
}

void UMultipleTask_MediaPlayback::PlayAudioAtLocation(USoundBase* Sound, const FVector& Location)
{
	if (!Sound || !GetWorld())
	{
		return;
	}

	CurrentAudioComponent = UGameplayStatics::SpawnSoundAtLocation(GetWorld(), Sound, Location, FRotator::ZeroRotator,
		1.f, 1.f, 0.f, nullptr, nullptr, false);

	if (CurrentAudioComponent)
	{
		CurrentAudioComponent->OnAudioFinished.AddDynamic(this, &UMultipleTask_MediaPlayback::OnAudioFinished);
	}
}

void UMultipleTask_MediaPlayback::PlayAudioOnPawn(USoundBase* Sound, APawn* Pawn)
{
	if (!Sound || !Pawn)
	{
		return;
	}

	// 查找或创建 AudioComponent
	UAudioComponent* AudioComp = Pawn->FindComponentByClass<UAudioComponent>();
	if (!AudioComp)
	{
		AudioComp = NewObject<UAudioComponent>(Pawn, FName(TEXT("AutoAudioComp")));
		AudioComp->RegisterComponent();
		AudioComp->SetIsReplicated(true);
	}

	// 解绑旧委托，防止重复绑定
	AudioComp->OnAudioFinished.RemoveDynamic(this, &UMultipleTask_MediaPlayback::OnAudioFinished);

	CurrentAudioComponent = AudioComp;
	AudioComp->SetSound(Sound);
	AudioComp->Play();
	AudioComp->OnAudioFinished.AddDynamic(this, &UMultipleTask_MediaPlayback::OnAudioFinished);
}

void UMultipleTask_MediaPlayback::PlayAnimOnPawn(UAnimSequenceBase* Anim, APawn* Pawn, int32 LoopCount)
{
	if (!Anim || !Pawn)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MediaPlayback] Pawn %s has no SkeletalMeshComponent, cannot play anim"),
			*GetNameSafe(Pawn));
		return;
	}

	const float PlayLength = Anim->GetPlayLength();
	Mesh->PlayAnimation(Anim, LoopCount == 0);

	// 设置动画时长定时器
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(AnimTimerHandle, this, &UMultipleTask_MediaPlayback::OnAnimTimerEnd,
			PlayLength, false);
	}
}

APawn* UMultipleTask_MediaPlayback::ResolveTargetPawn(const FMediaPlaybackItem& Item) const
{
	if (Item.LocationType != EMediaPlayLocationType::OnPawn)
	{
		return nullptr;
	}

	// 优先按 TargetRoleTag 查找
	if (!Item.TargetRoleTag.IsNone() && OwnerSequencer)
	{
		APawn* Pawn = OwnerSequencer->GetPawnByRoleTag(Item.TargetRoleTag);
		if (Pawn)
		{
			return Pawn;
		}
	}

	// 兜底用 TargetPawnRef
	if (Item.TargetPawnRef.IsValid())
	{
		return Cast<APawn>(Item.TargetPawnRef.Get());
	}

	return nullptr;
}

void UMultipleTask_MediaPlayback::OnItemFinished()
{
	const FMediaPlaybackItem& Item = Playlist[CurrentIndex];

	// 播放后延迟
	if (Item.DelayAfter > 0.f)
	{
		bIsPreDelay = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(DelayTimerHandle, this, &UMultipleTask_MediaPlayback::OnDelayTimerEnd,
				Item.DelayAfter, false);
		}
	}
	else
	{
		// 无延迟，直接推进
		AdvanceToNext();
	}
}

void UMultipleTask_MediaPlayback::AdvanceToNext()
{
	CleanupCurrentPlayback();
	CurrentIndex++;
	PlayCurrentItem();
}

void UMultipleTask_MediaPlayback::OnAudioFinished()
{
	UE_LOG(LogTemp, Verbose, TEXT("[MediaPlayback] Audio finished at item %d"), CurrentIndex);

	bAudioFinished = true;

	const FMediaPlaybackItem& Item = Playlist[CurrentIndex];
	// 没有动画，或动画已完成/不等待动画时，音频完成即本条目完成
	if (!Item.AnimSequence || bAnimFinished || !Item.bWaitAnimEnd)
	{
		OnItemFinished();
	}
	// 否则等动画定时器回调
}

void UMultipleTask_MediaPlayback::OnAnimTimerEnd()
{
	UE_LOG(LogTemp, Verbose, TEXT("[MediaPlayback] Anim timer end at item %d"), CurrentIndex);

	bAnimFinished = true;

	const FMediaPlaybackItem& Item = Playlist[CurrentIndex];
	// 没有音频，或音频已完成时，动画完成即本条目完成
	if (!Item.Sound || bAudioFinished)
	{
		OnItemFinished();
	}
	// 否则等音频回调
}

void UMultipleTask_MediaPlayback::OnDelayTimerEnd()
{
	UE_LOG(LogTemp, Verbose, TEXT("[MediaPlayback] Delay timer end at item %d, isPreDelay=%d"), CurrentIndex, bIsPreDelay ? 1 : 0);

	if (bIsPreDelay)
	{
		// 播放前延迟结束，开始真正播放
		bIsPreDelay = false;
		PlayNext();
	}
	else
	{
		// 播放后延迟结束，推进到下一条
		AdvanceToNext();
	}
}

void UMultipleTask_MediaPlayback::CleanupCurrentPlayback()
{
	if (CurrentAudioComponent)
	{
		CurrentAudioComponent->OnAudioFinished.RemoveDynamic(this, &UMultipleTask_MediaPlayback::OnAudioFinished);
		CurrentAudioComponent->Stop();
		CurrentAudioComponent = nullptr;
	}

	CurrentTargetPawn = nullptr;
	bIsPreDelay = false;
	bAudioFinished = false;
	bAnimFinished = false;
}
