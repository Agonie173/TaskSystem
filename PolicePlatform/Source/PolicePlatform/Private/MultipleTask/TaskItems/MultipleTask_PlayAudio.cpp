// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_PlayAudio.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

APawn* UMultipleTask_PlayAudio::ResolveTargetPawn() const
{
	if (!PlayAudioTaskInfo.IsValidIndex(PlayID))
	{
		return nullptr;
	}

	const FPlayAudioTaskInfo& Info = PlayAudioTaskInfo[PlayID];

	// 优先按 RoleTag 查玩家
	if (!Info.TargetRoleTag.IsNone())
	{
		return FindPawnByRoleTag(Info.TargetRoleTag);
	}

	// 否则直接引用
	return Info.PlayAudioPawn.Get();
}

void UMultipleTask_PlayAudio::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	PlayID = 0;
	if (!PlayAudioTaskInfo.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayAudio] Task started, %d audio item(s)"), PlayAudioTaskInfo.Num());
		StartPlayAudio();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayAudio] PlayAudioTaskInfo is empty, task ends immediately"));
		EndTask();
	}
}

void UMultipleTask_PlayAudio::EndTask_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DelayTimerHandle);
	}

	if (IsValid(CurrentAudioComponent))
	{
		CurrentAudioComponent->OnAudioFinished.RemoveDynamic(this, &UMultipleTask_PlayAudio::AudioFinished);
		CurrentAudioComponent->Stop();
	}
	CurrentAudioComponent = nullptr;

	Super::EndTask_Implementation();
}

void UMultipleTask_PlayAudio::StartPlayAudio()
{
	if (!PlayAudioTaskInfo.IsValidIndex(PlayID))
	{
		EndTask();
		return;
	}

	const FPlayAudioTaskInfo& Info = PlayAudioTaskInfo[PlayID];

	UE_LOG(LogTemp, Log, TEXT("[PlayAudio] Starting item %d/%d, delay before %.2fs"),
		PlayID + 1, PlayAudioTaskInfo.Num(), Info.DelayBeforePlayback);

	// 处理播放前延迟
	if (Info.DelayBeforePlayback > 0.f)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			World->GetTimerManager().SetTimer(DelayTimerHandle, this,
				&UMultipleTask_PlayAudio::OnDelayBeforePlayback, Info.DelayBeforePlayback, false);
			return;
		}
	}

	// 直接开始播放
	OnDelayBeforePlayback();
}

void UMultipleTask_PlayAudio::OnDelayBeforePlayback()
{
	if (bTaskEnded || !PlayAudioTaskInfo.IsValidIndex(PlayID))
	{
		return;
	}

	switch (PlayAudioTaskInfo[PlayID].PlayAudioType)
	{
	case EAudioPlayLocationType::PlayAudioForLocation:
		PlayAudioForLocation();
		break;
	case EAudioPlayLocationType::PlayAudioForPawn:
		PlayAudioForPawn();
		break;
	default:
		EndTask();
		break;
	}
}

void UMultipleTask_PlayAudio::PlayAnim(APawn* Pawn, UAnimSequence* PlayAudioSequence)
{
	if (!Pawn || !PlayAudioSequence)
	{
		return;
	}

	USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(
		Pawn->FindComponentByClass<USkeletalMeshComponent>());
	if (SkelMesh)
	{
		SkelMesh->PlayAnimation(PlayAudioSequence, false);
	}
}

void UMultipleTask_PlayAudio::PlayAudioForPawn()
{
	const FPlayAudioTaskInfo& Info = PlayAudioTaskInfo[PlayID];
	APawn* TargetPawn = ResolveTargetPawn();

	if (!Info.PlaySound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayAudio] PlaySound is null for PlayID=%d, skipped"), PlayID);
		AudioFinished();
		return;
	}

	if (!TargetPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayAudio] TargetPawn not found for PlayID=%d"), PlayID);
		AudioFinished();
		return;
	}

	// 查找或创建 AudioComponent
	UAudioComponent* AudioComp = TargetPawn->FindComponentByClass<UAudioComponent>();
	if (!AudioComp)
	{
		// 新建一个 AudioComponent 并注册到 Pawn
		AudioComp = NewObject<UAudioComponent>(TargetPawn, TEXT("AutoAudioComp"));
		if (AudioComp)
		{
			AudioComp->SetupAttachment(TargetPawn->GetRootComponent());
			AudioComp->SetIsReplicated(true);
			AudioComp->RegisterComponent();
		}
	}

	if (AudioComp)
	{
		CurrentAudioComponent = AudioComp;
		AudioComp->OnAudioFinished.RemoveDynamic(this, &UMultipleTask_PlayAudio::AudioFinished);
		AudioComp->OnAudioFinished.AddDynamic(this, &UMultipleTask_PlayAudio::AudioFinished);
		AudioComp->SetSound(Info.PlaySound);
		AudioComp->Play();

		UE_LOG(LogTemp, Log, TEXT("[PlayAudio] Playing sound %s on pawn %s (duration %.2fs, PlayID=%d)"),
			*Info.PlaySound->GetName(), *TargetPawn->GetName(), Info.PlaySound->GetDuration(), PlayID);

		if (Info.bNeedPlayAnim)
		{
			PlayAnim(TargetPawn, Info.PlayAudioSequence);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayAudio] Failed to create AudioComponent on %s"), *TargetPawn->GetName());
		AudioFinished();
	}
}

void UMultipleTask_PlayAudio::PlayAudioForLocation()
{
	const FPlayAudioTaskInfo& Info = PlayAudioTaskInfo[PlayID];

	UWorld* World = GetWorld();
	if (!World || !Info.PlaySound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayAudio] PlayAudioForLocation skipped (world %d, sound %d, PlayID=%d)"),
			World != nullptr, Info.PlaySound != nullptr, PlayID);
		AudioFinished();
		return;
	}

	// 用 SpawnSoundAtLocation 生成“挂在 World 上”的音频组件：
	// NewObject<UAudioComponent>(this) 的 Outer 是 UObject，组件无法正确注册到世界。
	UAudioComponent* SpawnedAudio = UGameplayStatics::SpawnSoundAtLocation(
		World, Info.PlaySound, Info.PlayAudioLocation, FRotator::ZeroRotator,
		1.f, 1.f, 0.f, nullptr, nullptr, /*bAutoDestroy=*/true);

	if (SpawnedAudio)
	{
		CurrentAudioComponent = SpawnedAudio;
		SpawnedAudio->OnAudioFinished.RemoveDynamic(this, &UMultipleTask_PlayAudio::AudioFinished);
		SpawnedAudio->OnAudioFinished.AddDynamic(this, &UMultipleTask_PlayAudio::AudioFinished);

		UE_LOG(LogTemp, Log, TEXT("[PlayAudio] Playing sound %s at %s (duration %.2fs, PlayID=%d)"),
			*Info.PlaySound->GetName(), *Info.PlayAudioLocation.ToCompactString(),
			Info.PlaySound->GetDuration(), PlayID);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayAudio] SpawnSoundAtLocation failed for sound %s"),
			*Info.PlaySound->GetName());
		AudioFinished();
	}
}

void UMultipleTask_PlayAudio::AudioFinished()
{
	if (bTaskEnded)
	{
		return;
	}

	// 清理旧的 AudioComponent 引用
	if (IsValid(CurrentAudioComponent))
	{
		CurrentAudioComponent->OnAudioFinished.RemoveDynamic(this, &UMultipleTask_PlayAudio::AudioFinished);
	}
	CurrentAudioComponent = nullptr;

	// 播放后延迟属于「刚刚播完的这一条」，必须在 PlayID++ 之前读取
	const float DelayAfter = PlayAudioTaskInfo.IsValidIndex(PlayID)
		? PlayAudioTaskInfo[PlayID].DelayAfterPlayback
		: 0.f;

	UE_LOG(LogTemp, Log, TEXT("[PlayAudio] Item %d finished (delay after %.2fs)"), PlayID, DelayAfter);

	PlayID++;
	if (PlayAudioTaskInfo.IsValidIndex(PlayID))
	{
		// 处理播放后延迟
		if (DelayAfter > 0.f)
		{
			UWorld* World = GetWorld();
			if (World)
			{
				World->GetTimerManager().SetTimer(DelayTimerHandle, this,
					&UMultipleTask_PlayAudio::OnDelayAfterPlayback, DelayAfter, false);
				return;
			}
		}
		StartPlayAudio();
	}
	else
	{
		EndTask();
	}
}

void UMultipleTask_PlayAudio::OnDelayAfterPlayback()
{
	if (bTaskEnded)
	{
		return;
	}
	StartPlayAudio();
}
