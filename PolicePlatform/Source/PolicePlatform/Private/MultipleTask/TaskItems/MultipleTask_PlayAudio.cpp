// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_PlayAudio.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

APawn* UMultipleTask_PlayAudio::ResolveTargetPawn() const
{
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
	PlayID = 0;
	if (!PlayAudioTaskInfo.IsEmpty())
	{
		StartPlayAudio();
	}
	else
	{
		EndTask();
	}
}

void UMultipleTask_PlayAudio::EndTask_Implementation()
{
	if (CurrentAudioComponent)
	{
		CurrentAudioComponent->OnAudioFinished.RemoveDynamic(this, &UMultipleTask_PlayAudio::AudioFinished);
		CurrentAudioComponent->Stop();
	}
	Super::EndTask_Implementation();
}

void UMultipleTask_PlayAudio::StartPlayAudio()
{
	const FPlayAudioTaskInfo& Info = PlayAudioTaskInfo[PlayID];

	// 处理播放前延迟
	if (Info.DelayBeforePlayback > 0.f)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FTimerHandle DummyHandle;
			World->GetTimerManager().SetTimer(DummyHandle, this, &UMultipleTask_PlayAudio::OnDelayBeforePlayback,
				Info.DelayBeforePlayback, false);
			return;
		}
	}

	// 直接开始播放
	OnDelayBeforePlayback();
}

void UMultipleTask_PlayAudio::OnDelayBeforePlayback()
{
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
		AudioComp->SetSound(Info.PlaySound);
		AudioComp->OnAudioFinished.AddDynamic(this, &UMultipleTask_PlayAudio::AudioFinished);
		AudioComp->Play();

		if (Info.bNeedPlayAnim)
		{
			PlayAnim(TargetPawn, Info.PlayAudioSequence);
		}
	}
	else
	{
		AudioFinished();
	}
}

void UMultipleTask_PlayAudio::PlayAudioForLocation()
{
	const FPlayAudioTaskInfo& Info = PlayAudioTaskInfo[PlayID];

	UWorld* World = GetWorld();
	if (!World)
	{
		AudioFinished();
		return;
	}

	// 在指定位置 Spawn 临时 AudioComponent
	FAudioDeviceHandle AudioDevice = World->GetAudioDevice();
	if (!AudioDevice.IsValid())
	{
		AudioFinished();
		return;
	}

	UAudioComponent* SpawnedAudio = NewObject<UAudioComponent>(this);
	if (SpawnedAudio)
	{
		SpawnedAudio->SetWorldLocation(Info.PlayAudioLocation);
		SpawnedAudio->SetSound(Info.PlaySound);
		SpawnedAudio->bAutoDestroy=true;
		SpawnedAudio->RegisterComponent();
		SpawnedAudio->Activate();

		CurrentAudioComponent = SpawnedAudio;
		SpawnedAudio->OnAudioFinished.AddDynamic(this, &UMultipleTask_PlayAudio::AudioFinished);
	}
	else
	{
		AudioFinished();
	}
}

void UMultipleTask_PlayAudio::AudioFinished()
{
	// 清理旧的 AudioComponent 引用
	if (CurrentAudioComponent)
	{
		CurrentAudioComponent->OnAudioFinished.RemoveDynamic(this, &UMultipleTask_PlayAudio::AudioFinished);
		CurrentAudioComponent = nullptr;
	}

	PlayID++;
	if (PlayAudioTaskInfo.IsValidIndex(PlayID))
	{
		// 处理播放后延迟
		if (PlayAudioTaskInfo[PlayID].DelayAfterPlayback > 0.f)
		{
			UWorld* World = GetWorld();
			if (World)
			{
				FTimerHandle DummyHandle;
				World->GetTimerManager().SetTimer(DummyHandle, this, &UMultipleTask_PlayAudio::OnDelayAfterPlayback,
					PlayAudioTaskInfo[PlayID].DelayAfterPlayback, false);
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
	StartPlayAudio();
}
