// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_PlayAnim.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMultipleTask_PlayAnim::StartTask_Implementation()
{
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

	CurrentAnimIndex = 0;
	CurrentLoopCount = 0;

	if (!bWaitAnimEnd)
	{
		PlayNextAnim();
		EndTask();
	}
	else
	{
		PlayNextAnim();
	}
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
		return;
	}

	USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(
		CachedTargetPawn->FindComponentByClass<USkeletalMeshComponent>());
	if (!SkelMesh)
	{
		OnSingleAnimEnd();
		return;
	}

	// 播放动画
	if (UAnimSequence* Seq = Cast<UAnimSequence>(AnimToPlay))
	{
		SkelMesh->PlayAnimation(Seq, false);
	}
	else if (UAnimMontage* Montage = Cast<UAnimMontage>(AnimToPlay))
	{
		SkelMesh->PlayAnimation(Montage, false);
	}

	// UE 5.3 UAnimInstance 没有 OnAnimationEnded 委托
	// 用 Timer 等待动画时长
	UWorld* World = GetWorld();
	if (World)
	{
		float Duration = AnimToPlay->GetPlayLength();
		FTimerHandle DummyHandle;
		World->GetTimerManager().SetTimer(DummyHandle, this, &UMultipleTask_PlayAnim::OnSingleAnimEnd,
			Duration, false);
	}
	else
	{
		OnSingleAnimEnd();
	}
}

void UMultipleTask_PlayAnim::OnSingleAnimEnd()
{
	CurrentAnimIndex++;

	bool bHasMoreAnims = false;
	if (bNeedMultipleAnim)
	{
		bHasMoreAnims = PlayAnimInfoArray.IsValidIndex(CurrentAnimIndex);
	}

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
