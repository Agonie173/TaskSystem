// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_EditActorTag.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "MultipleTask/TaskAudioBroadcaster.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void UMultipleTask_EditActorTag::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	UWorld* World = GetWorld();
	if (FindTag.IsNone() || TagToModify.IsNone() || !World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EditActorTag] FindTag or TagToModify is None, task ends immediately"));
		EndTask();
		return;
	}

	const bool bAdd = (Operation == EActorTagOperation::AddTag);
	int32 ModifiedCount = 0;

	// 服务器解算 + 本地修改：所有带 FindTag 的复制 Actor
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->ActorHasTag(FindTag))
		{
			continue;
		}

		// Tags 不复制：仅处理复制 Actor，保证两端一致（非复制 Actor 客户端无实例可同步）
		if (!Actor->GetIsReplicated())
		{
			UE_LOG(LogTemp, Warning, TEXT("[EditActorTag] %s is not replicated, skipped to keep server/client consistent"),
				*Actor->GetName());
			continue;
		}

		if (bAdd)
		{
			if (!Actor->Tags.Contains(TagToModify))
			{
				Actor->Tags.Add(TagToModify);
			}
		}
		else
		{
			Actor->Tags.Remove(TagToModify);
		}
		ModifiedCount++;

		UE_LOG(LogTemp, Log, TEXT("[EditActorTag] Server %s tag %s on %s"),
			bAdd ? TEXT("added") : TEXT("removed"), *TagToModify.ToString(), *Actor->GetName());

		// 广播到客户端（Actor 引用参数由引擎跨机自动解析）
		if (OwnerSequencer)
		{
			if (ATaskAudioBroadcaster* Broadcaster = OwnerSequencer->GetAudioBroadcaster())
			{
				Broadcaster->BroadcastModifyActorTags(Actor, TagToModify, bAdd);
			}
		}
	}

	if (ModifiedCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EditActorTag] No replicated actor matched tag %s, nothing modified"), *FindTag.ToString());
	}

	// 一次性动作：改完即结束（非阻塞任务，结束不影响步骤推进）
	EndTask();
}
