// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_PlayAudio.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "MultipleTask/TaskAudioBroadcaster.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ATaskAudioBroadcaster* UMultipleTask_PlayAudio::GetBroadcaster() const
{
	return OwnerSequencer ? OwnerSequencer->GetAudioBroadcaster() : nullptr;
}

void UMultipleTask_PlayAudio::BindAudioFinished(UAudioComponent* AudioComp)
{
	if (IsValid(CurrentAudioComponent))
	{
		CurrentAudioComponent->OnAudioFinished.RemoveDynamic(this, &UMultipleTask_PlayAudio::AudioFinished);
	}
	CurrentAudioComponent = AudioComp;
	// 复用已有组件时防重复绑定
	if (AudioComp)
	{
		AudioComp->OnAudioFinished.RemoveDynamic(this, &UMultipleTask_PlayAudio::AudioFinished);
		AudioComp->OnAudioFinished.AddDynamic(this, &UMultipleTask_PlayAudio::AudioFinished);
	}
}

APawn* UMultipleTask_PlayAudio::ResolveTargetPawn() const
{
	if (!PlayAudioTaskInfo.IsValidIndex(PlayID))
	{
		return nullptr;
	}
	const FPlayAudioTaskInfo& Info = PlayAudioTaskInfo[PlayID];
	return !Info.TargetRoleTag.IsNone() ? FindPawnByRoleTag(Info.TargetRoleTag) : Info.PlayAudioPawn.Get();
}

void UMultipleTask_PlayAudio::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	PlayID = 0;
	if (PlayAudioTaskInfo.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayAudio] PlayAudioTaskInfo is empty, task ends immediately"));
		EndTask();
		return;
	}
	StartPlayAudio();
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

	// 凭句柄广播停止客户端侧音频（步骤提前结束时客户端声音随之静音）
	if (LastAudioHandle != INDEX_NONE)
	{
		if (ATaskAudioBroadcaster* Broadcaster = GetBroadcaster())
		{
			Broadcaster->BroadcastStopAudio(LastAudioHandle);
		}
		LastAudioHandle = INDEX_NONE;
	}
	Super::EndTask_Implementation();
}

void UMultipleTask_PlayAudio::StartPlayAudio()
{
	if (!PlayAudioTaskInfo.IsValidIndex(PlayID))
	{
		EndTask();
		return;
	}

	// 播放前延时（EndTask 会清掉本定时器，回调无需再判任务状态）
	const float DelayBefore = PlayAudioTaskInfo[PlayID].DelayBeforePlayback;
	if (DelayBefore > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(DelayTimerHandle, this,
			&UMultipleTask_PlayAudio::PlayCurrentItem, DelayBefore, false);
		return;
	}
	PlayCurrentItem();
}

void UMultipleTask_PlayAudio::PlayCurrentItem()
{
	const FPlayAudioTaskInfo& Info = PlayAudioTaskInfo[PlayID];
	if (!Info.PlaySound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayAudio] Item %d has no sound, skipped"), PlayID);
		AudioFinished();
		return;
	}

	switch (Info.PlayAudioType)
	{
	case EAudioPlayLocationType::PlayAudioForPawn:
		PlayAudioForPawn();
		break;
	case EAudioPlayLocationType::PlayAudioForLocation:
	default:
		PlayAudioForLocation();
		break;
	}
}

void UMultipleTask_PlayAudio::PlayAudioForPawn()
{
	const FPlayAudioTaskInfo& Info = PlayAudioTaskInfo[PlayID];
	APawn* TargetPawn = ResolveTargetPawn();
	if (!TargetPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayAudio] Target pawn not found for item %d, skipped"), PlayID);
		AudioFinished();
		return;
	}

	// 查找或动态创建 AudioComponent
	UAudioComponent* AudioComp = TargetPawn->FindComponentByClass<UAudioComponent>();
	if (!AudioComp)
	{
		AudioComp = NewObject<UAudioComponent>(TargetPawn, TEXT("AutoAudioComp"));
		AudioComp->SetupAttachment(TargetPawn->GetRootComponent());
		AudioComp->RegisterComponent();
	}
	BindAudioFinished(AudioComp);
	AudioComp->SetSound(Info.PlaySound);
	AudioComp->Play();

	UE_LOG(LogTemp, Log, TEXT("[PlayAudio] Playing %s on %s (item %d, duration %.2fs)"),
		*Info.PlaySound->GetName(), *TargetPawn->GetName(), PlayID, Info.PlaySound->GetDuration());

	if (ATaskAudioBroadcaster* Broadcaster = GetBroadcaster())
	{
		LastAudioHandle = Broadcaster->BroadcastPlayAudio(Info.PlaySound, TargetPawn, FVector::ZeroVector,
			1.f, 1.f, Info.bNeedPlayAnim, Info.PlayAudioSequence);
	}
	if (Info.bNeedPlayAnim)
	{
		PlayAnim(TargetPawn, Info.PlayAudioSequence);
	}
}

void UMultipleTask_PlayAudio::PlayAudioForLocation()
{
	const FPlayAudioTaskInfo& Info = PlayAudioTaskInfo[PlayID];
	UAudioComponent* SpawnedAudio = UGameplayStatics::SpawnSoundAtLocation(
		GetWorld(), Info.PlaySound, Info.PlayAudioLocation, FRotator::ZeroRotator);
	if (!SpawnedAudio)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayAudio] SpawnSoundAtLocation failed for item %d, skipped"), PlayID);
		AudioFinished();
		return;
	}
	BindAudioFinished(SpawnedAudio);

	UE_LOG(LogTemp, Log, TEXT("[PlayAudio] Playing %s at %s (item %d, duration %.2fs)"),
		*Info.PlaySound->GetName(), *Info.PlayAudioLocation.ToCompactString(), PlayID, Info.PlaySound->GetDuration());

	if (ATaskAudioBroadcaster* Broadcaster = GetBroadcaster())
	{
		LastAudioHandle = Broadcaster->BroadcastPlayAudio(Info.PlaySound, nullptr, Info.PlayAudioLocation,
			1.f, 1.f, false, nullptr);
	}
}

void UMultipleTask_PlayAudio::PlayAnim(APawn* Pawn, UAnimSequence* PlayAudioSequence)
{
	if (!Pawn || !PlayAudioSequence)
	{
		return;
	}
	// 播放统一由广播通道驱动（Slot 蒙太奇，服务器本地 + 客户端重建，自动混合回蓝图）
	if (ATaskAudioBroadcaster* Broadcaster = GetBroadcaster())
	{
		Broadcaster->BroadcastPlayAnim(Pawn, PlayAudioSequence);
	}
}

void UMultipleTask_PlayAudio::AudioFinished()
{
	// EndTask 先解绑再 Stop，能到达此处的只有任务结束后的残留回调
	if (bTaskEnded)
	{
		return;
	}

	BindAudioFinished(nullptr);

	// 播放后延时属于「刚播完的这一条」，须在 PlayID++ 之前读取
	const float DelayAfter = PlayAudioTaskInfo.IsValidIndex(PlayID)
		? PlayAudioTaskInfo[PlayID].DelayAfterPlayback
		: 0.f;

	PlayID++;
	if (!PlayAudioTaskInfo.IsValidIndex(PlayID))
	{
		EndTask();
		return;
	}

	// 播放后延时到期后走 StartPlayAudio（会再处理下一条的播放前延时）
	if (DelayAfter > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(DelayTimerHandle, this,
			&UMultipleTask_PlayAudio::StartPlayAudio, DelayAfter, false);
		return;
	}
	StartPlayAudio();
}
