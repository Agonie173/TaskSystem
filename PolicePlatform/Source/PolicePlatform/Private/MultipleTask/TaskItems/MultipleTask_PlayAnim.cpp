// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_PlayAnim.h"
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

	USkeletalMeshComponent* SkelMesh =
		CachedTargetPawn->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayAnim] %s has no SkeletalMeshComponent"), *CachedTargetPawn->GetName());
		return false;
	}

	if (UAnimSequence* Seq = Cast<UAnimSequence>(AnimToPlay))
	{
		SkelMesh->PlayAnimation(Seq, false);
		UE_LOG(LogTemp, Log, TEXT("[PlayAnim] Playing sequence %s on %s"),
			*Seq->GetName(), *CachedTargetPawn->GetName());
		return true;
	}

	if (UAnimMontage* Montage = Cast<UAnimMontage>(AnimToPlay))
	{
		SkelMesh->PlayAnimation(Montage, false);
		UE_LOG(LogTemp, Log, TEXT("[PlayAnim] Playing montage %s on %s"),
			*Montage->GetName(), *CachedTargetPawn->GetName());
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[PlayAnim] Unsupported animation type: %s"), *AnimToPlay->GetClass()->GetName());
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
		// 没有可播动画时不能靠递归重试：LoopCount=0 会变成无限同步递归直接爆栈
		UE_LOG(LogTemp, Warning, TEXT("[PlayAnim] No animation at index %d, task ending"), CurrentAnimIndex);
		EndTask();
		return;
	}

	if (!PlayAnimOnTarget(AnimToPlay))
	{
		// 目标上播不了动画时直接结束，避免循环重试（LoopCount=0 会无限同步递归）
		EndTask();
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		const float Duration = FMath::Max(AnimToPlay->GetPlayLength(), 0.01f);
		UE_LOG(LogTemp, Verbose, TEXT("[PlayAnim] Wait %.2fs for anim end (index %d)"), Duration, CurrentAnimIndex);
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

	UE_LOG(LogTemp, Log, TEXT("[PlayAnim] Segment ended (next index %d, loop %d, loop count %d)"),
		CurrentAnimIndex, CurrentLoopCount, LoopCount);

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
