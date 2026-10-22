// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_PlayAnim.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "MultipleTask/TaskAudioBroadcaster.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMultipleTask_PlayAnim::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	CachedTargetPawn = nullptr;
	if (!TargetRoleTag.IsNone())
	{
		CachedTargetPawn = FindPawnByRoleTag(TargetRoleTag);
	}
	else if (!TargetPawnRef.IsNull())
	{
		CachedTargetPawn = TargetPawnRef.Get();
	}

	if (!CachedTargetPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayAnim] TargetPawn not found"));
		EndTask();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayAnim] Target pawn: %s"), *CachedTargetPawn->GetName());
	CurrentAnimIndex = 0;
	CurrentLoopCount = 0;
	PlayNextAnim();
	if (!bWaitAnimEnd)
	{
		EndTask();
	}
}

void UMultipleTask_PlayAnim::EndTask_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AnimTimerHandle);
	}
	Super::EndTask_Implementation();
}

bool UMultipleTask_PlayAnim::PlayAnimOnTarget(UAnimSequenceBase* AnimToPlay)
{
	if (!CachedTargetPawn || !AnimToPlay)
	{
		return false;
	}

	if (!AnimToPlay->IsA<UAnimSequence>() && !AnimToPlay->IsA<UAnimMontage>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayAnim] Unsupported animation type: %s"), *AnimToPlay->GetClass()->GetName());
		return false;
	}

	// 播放统一由广播通道驱动（Slot 蒙太奇，服务器本地 + 客户端重建，自动混合回蓝图）
	if (OwnerSequencer && OwnerSequencer->GetAudioBroadcaster())
	{
		OwnerSequencer->GetAudioBroadcaster()->BroadcastPlayAnim(CachedTargetPawn, AnimToPlay);
		UE_LOG(LogTemp, Log, TEXT("[PlayAnim] Playing %s on %s"),
			*AnimToPlay->GetName(), *CachedTargetPawn->GetName());
		return true;
	}

	// 通道缺失兜底：本地裸播放（无混合，仅单机调试）
	if (USkeletalMeshComponent* SkelMesh = CachedTargetPawn->FindComponentByClass<USkeletalMeshComponent>())
	{
		SkelMesh->PlayAnimation(AnimToPlay, false);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[PlayAnim] %s has no SkeletalMeshComponent"), *CachedTargetPawn->GetName());
	return false;
}

void UMultipleTask_PlayAnim::PlayNextAnim()
{
	if (!CachedTargetPawn)
	{
		EndTask();
		return;
	}

	UAnimSequenceBase* AnimToPlay = nullptr;
	if (!bNeedMultipleAnim)
	{
		AnimToPlay = PlayAnimInfo.PlayAnimSequence;
	}
	else if (PlayAnimInfoArray.IsValidIndex(CurrentAnimIndex))
	{
		AnimToPlay = PlayAnimInfoArray[CurrentAnimIndex].PlayAnimSequence;
	}

	if (!AnimToPlay)
	{
		// 不能靠递归重试：LoopCount=0 会变成无限同步递归直接爆栈
		UE_LOG(LogTemp, Warning, TEXT("[PlayAnim] No animation at index %d, task ending"), CurrentAnimIndex);
		EndTask();
		return;
	}

	if (!PlayAnimOnTarget(AnimToPlay))
	{
		// 播不了直接结束，避免循环重试（原因同上）
		EndTask();
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		const float Duration = FMath::Max(AnimToPlay->GetPlayLength(), 0.01f);
		World->GetTimerManager().SetTimer(AnimTimerHandle, this,
			&UMultipleTask_PlayAnim::OnSingleAnimEnd, Duration, false);
	}
	else
	{
		OnSingleAnimEnd();
	}
}

void UMultipleTask_PlayAnim::OnSingleAnimEnd()
{
	CurrentAnimIndex++;

	bool bHasMoreAnims = bNeedMultipleAnim && PlayAnimInfoArray.IsValidIndex(CurrentAnimIndex);

	if (bHasMoreAnims)
	{
		PlayNextAnim();
	}
	else
	{
		CurrentLoopCount++;
		if (LoopCount > 0 && CurrentLoopCount >= LoopCount)
		{
			EndTask();
		}
		else
		{
			CurrentAnimIndex = 0;
			PlayNextAnim();
		}
	}
}
