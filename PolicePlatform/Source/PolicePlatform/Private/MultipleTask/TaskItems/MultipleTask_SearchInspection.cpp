// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_SearchInspection.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

void UMultipleTask_SearchInspection::StartTask_Implementation()
{
	// 查找目标 Actor
	CachedTargetActor = FindActorByTag(TargetActorTag);
	if (!CachedTargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SearchInspection] Target Actor with tag %s not found"),
			*TargetActorTag.ToString());
		EndTask();
		return;
	}

	if (BodyParts.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SearchInspection] BodyParts is empty"));
		EndTask();
		return;
	}

	CurrentPartIndex = 0;
	TotalPartsChecked = 0;
	HighlightCurrentPart();

	UE_LOG(LogTemp, Log, TEXT("[SearchInspection] Started. Target=%s, Parts=%d"),
		*CachedTargetActor->GetName(), BodyParts.Num());
}

void UMultipleTask_SearchInspection::EndTask_Implementation()
{
	ClearHighlightCurrentPart();

	// 显示搜身结果
	if (!SearchResultText.ToString().IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("[SearchInspection] Result: %s"), *SearchResultText.ToString());
	}

	Super::EndTask_Implementation();
}

void UMultipleTask_SearchInspection::OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!OtherActor || !CachedTargetActor)
	{
		return;
	}

	if (CurrentPartIndex < 0 || CurrentPartIndex >= BodyParts.Num())
	{
		return;
	}

	const FBodyPartConfig& CurrentPart = BodyParts[CurrentPartIndex];

	// 检查触发的碰撞盒是不是当前部位的
	if (!CurrentPart.CollisionBoxTag.IsNone())
	{
		if (!OverlappedActor->ActorHasTag(CurrentPart.CollisionBoxTag))
		{
			return; // 不是当前部位的盒子，忽略
		}
	}

	// 检查触发者是否有资格（InspectorRoleTags 为空则所有人都可以）
	bool bValidInspector = InspectorRoleTags.IsEmpty();
	if (!bValidInspector)
	{
		for (const FName& Tag : OtherActor->Tags)
		{
			if (InspectorRoleTags.Contains(Tag))
			{
				bValidInspector = true;
				break;
			}
		}
	}

	if (!bValidInspector)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[SearchInspection] Part '%s' checked by %s"),
		*CurrentPart.PartName.ToString(), *OtherActor->GetName());

	// 播放单部位完成音效
	if (PartCompleteSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), PartCompleteSound,
			CachedTargetActor->GetActorLocation());
	}

	AdvanceToNextPart();
}

void UMultipleTask_SearchInspection::HighlightCurrentPart()
{
	if (CurrentPartIndex < 0 || CurrentPartIndex >= BodyParts.Num())
	{
		return;
	}

	const FBodyPartConfig& CurrentPart = BodyParts[CurrentPartIndex];
	UE_LOG(LogTemp, Log, TEXT("[SearchInspection] Highlighting part %d/%d: '%s'"),
		CurrentPartIndex + 1, BodyParts.Num(), *CurrentPart.PartName.ToString());

	// 高亮的实际实现可以是：
	// 1. 给 Target 的 Mesh 对应 Socket 位置上 Spawn 一个高亮 Actor
	// 2. 或者动态材质参数 bHighlight=true
	// 这里留接口给蓝图/Sequencer 层实现
}

void UMultipleTask_SearchInspection::ClearHighlightCurrentPart()
{
	// 清除上一个部位的高亮
}

void UMultipleTask_SearchInspection::AdvanceToNextPart()
{
	TotalPartsChecked++;
	ClearHighlightCurrentPart();
	CurrentPartIndex++;

	if (CurrentPartIndex >= BodyParts.Num())
	{
		// 所有部位完成
		UE_LOG(LogTemp, Log, TEXT("[SearchInspection] All %d parts checked!"), TotalPartsChecked);

		// 播放全部完成音效
		if (AllCompleteSound)
		{
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), AllCompleteSound,
				CachedTargetActor->GetActorLocation());
		}

		EndTask();
	}
	else
	{
		HighlightCurrentPart();
	}
}
