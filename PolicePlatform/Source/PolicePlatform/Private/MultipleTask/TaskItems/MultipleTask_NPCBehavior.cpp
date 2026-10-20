// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_NPCBehavior.h"
#include "AIController.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "MultipleTask/TaskSequencerComponent.h"

void UMultipleTask_NPCBehavior::StartTask_Implementation()
{
	if (bUseExistingNPC)
	{
		// 接管模式：按 Tag 查找已有 NPC
		for (APawn* Pawn : OwnerSequencer->GetAllPlayerPawns())
		{
			// 跳过玩家自己
		}

		// 遍历 World 查找带 Tag 的 Pawn
		UWorld* World = GetWorld();
		if (World)
		{
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				APawn* P = *It;
				if (P && !P->IsPlayerControlled() && P->ActorHasTag(ExistingNPCTag))
				{
					ControlledNPCs.Add(P);
				}
			}
		}

		if (ControlledNPCs.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[NPCBehavior] No existing NPC found with tag %s"),
				*ExistingNPCTag.ToString());
			EndTask();
			return;
		}
	}
	else
	{
		// Spawn 模式
		if (NPCClass.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[NPCBehavior] NPCClass not set"));
			EndTask();
			return;
		}

		UWorld* World = GetWorld();
		if (!World)
		{
			EndTask();
			return;
		}

		// 解析 Spawn 位置
		FVector Loc = SpawnLocation;
		FRotator Rot = SpawnRotation;

		if (!SpawnPointTag.IsNone())
		{
			AActor* SpawnPoint = FindActorByTag(SpawnPointTag);
			if (SpawnPoint)
			{
				Loc = SpawnPoint->GetActorLocation();
				Rot = SpawnPoint->GetActorRotation();
			}
		}

		// Spawn 多个 NPC
		for (int32 i = 0; i < NPCCount; i++)
		{
			FVector OffsetLoc = Loc + FVector(i * 100.f, 0, 0);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			APawn* NewNPC = World->SpawnActor<APawn>(NPCClass.LoadSynchronous(), OffsetLoc, Rot, Params);
			if (NewNPC)
			{
				ControlledNPCs.Add(NewNPC);
			}
		}

		if (ControlledNPCs.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[NPCBehavior] Failed to spawn any NPC"));
			EndTask();
			return;
		}
	}

	CurrentBehaviorIndex = -1;
	UE_LOG(LogTemp, Log, TEXT("[NPCBehavior] Started with %d NPCs, %d behavior nodes"),
		ControlledNPCs.Num(), BehaviorSequence.Num());

	ExecuteNextNode();
}

void UMultipleTask_NPCBehavior::EndTask_Implementation()
{
	if (bAutoDestroyOnEnd)
	{
		for (APawn* NPC : ControlledNPCs)
		{
			if (NPC)
			{
				NPC->Destroy();
			}
		}
	}
	ControlledNPCs.Empty();
	Super::EndTask_Implementation();
}

void UMultipleTask_NPCBehavior::ExecuteNextNode()
{
	CurrentBehaviorIndex++;

	if (!BehaviorSequence.IsValidIndex(CurrentBehaviorIndex))
	{
		// 所有行为节点完成
		UE_LOG(LogTemp, Log, TEXT("[NPCBehavior] All behavior nodes completed"));
		EndTask();
		return;
	}

	const FNPCBehaviorNode& Node = BehaviorSequence[CurrentBehaviorIndex];
	UE_LOG(LogTemp, Log, TEXT("[NPCBehavior] Executing node %d: %d"), CurrentBehaviorIndex, (int32)Node.NodeType);

	switch (Node.NodeType)
	{
	case ENPCBehaviorType::MoveTo:
	{
		FVector MoveTarget = Node.TargetLocation;
		if (!Node.TargetLocationTag.IsNone())
		{
			AActor* TargetActor = FindActorByTag(Node.TargetLocationTag);
			if (TargetActor)
			{
				MoveTarget = TargetActor->GetActorLocation();
			}
		}

		for (APawn* NPC : ControlledNPCs)
		{
			if (!NPC) continue;
			AController* Ctrl = NPC->GetController();
			if (AAIController* AICtrl = Cast<AAIController>(Ctrl))
			{
				AICtrl->MoveToLocation(MoveTarget, Node.AcceptanceRadius);
				// 绑定完成委托
				AICtrl->ReceiveMoveCompleted.AddDynamic(this, &UMultipleTask_NPCBehavior::OnMoveCompleted);
			}
		}

		// 如果没有 AIController 或没有 NPC，直接跳过
		if (ControlledNPCs.IsEmpty())
		{
			AdvanceToNextNode();
		}
		break;
	}
	case ENPCBehaviorType::Wait:
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FTimerHandle Dummy;
			World->GetTimerManager().SetTimer(Dummy, this, &UMultipleTask_NPCBehavior::AdvanceToNextNode,
				Node.WaitDuration, false);
		}
		else
		{
			AdvanceToNextNode();
		}
		break;
	}
	case ENPCBehaviorType::PlayAnim:
	{
		for (APawn* NPC : ControlledNPCs)
		{
			if (!NPC) continue;
			USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(
				NPC->FindComponentByClass<USkeletalMeshComponent>());
			if (SkelMesh && Node.AnimToPlay)
			{
				SkelMesh->PlayAnimation(Node.AnimToPlay, false);
			}
		}
		AdvanceToNextNode();
		break;
	}
	case ENPCBehaviorType::PlayAudio:
	{
		bool bStarted = false;
		for (APawn* NPC : ControlledNPCs)
		{
			if (!NPC) continue;

			UAudioComponent* AudioComp = NPC->FindComponentByClass<UAudioComponent>();
			if (!AudioComp)
			{
				AudioComp = NewObject<UAudioComponent>(NPC, TEXT("AutoAudioComp"));
				if (AudioComp)
				{
					AudioComp->SetupAttachment(NPC->GetRootComponent());
					AudioComp->SetIsReplicated(true);
					AudioComp->RegisterComponent();
				}
			}

			if (AudioComp && Node.AudioToPlay)
			{
				AudioComp->SetSound(Node.AudioToPlay);
				AudioComp->OnAudioFinished.AddDynamic(this, &UMultipleTask_NPCBehavior::OnPlayAudioFinished);
				AudioComp->Play();
				bStarted = true;
				break; // 只等第一个 NPC 的音频播完
			}
		}
		if (!bStarted)
		{
			AdvanceToNextNode();
		}
		break;
	}
	case ENPCBehaviorType::LookAt:
	{
		// 简化处理：朝向目标 Actor 位置
		FVector LookLoc = FVector::ZeroVector;
		if (!Node.LookAtTargetTag.IsNone())
		{
			AActor* Target = FindActorByTag(Node.LookAtTargetTag);
			if (Target) LookLoc = Target->GetActorLocation();
		}

		for (APawn* NPC : ControlledNPCs)
		{
			if (!NPC) continue;
			FVector Dir = (LookLoc - NPC->GetActorLocation());
			Dir.Z = 0;
			if (!Dir.IsNearlyZero())
			{
				NPC->SetActorRotation(Dir.Rotation());
			}
		}
		AdvanceToNextNode();
		break;
	}
	case ENPCBehaviorType::SetAIState:
	{
		// 通过自定义接口调用 NPC 的 AI 状态切换
		for (APawn* NPC : ControlledNPCs)
		{
			if (!NPC) continue;
			// 如果 NPC 有自定义接口，可以 Cast 后调用 SetAIState
			// 这里做的是通用处理
			UE_LOG(LogTemp, Log, TEXT("[NPCBehavior] SetAIState '%s' on %s"),
				*Node.AIStateName.ToString(), *NPC->GetName());
		}
		AdvanceToNextNode();
		break;
	}
	case ENPCBehaviorType::Destroy:
	{
		for (APawn* NPC : ControlledNPCs)
		{
			if (NPC) NPC->Destroy();
		}
		ControlledNPCs.Empty();
		AdvanceToNextNode();
		break;
	}
	default:
		AdvanceToNextNode();
		break;
	}
}

void UMultipleTask_NPCBehavior::OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	// 所有 AIController 都解绑 MoveCompleted 委托
	for (APawn* NPC : ControlledNPCs)
	{
		if (!NPC) continue;
		if (AController* Ctrl = NPC->GetController())
		{
			if (AAIController* AICtrl = Cast<AAIController>(Ctrl))
			{
				AICtrl->ReceiveMoveCompleted.RemoveDynamic(this, &UMultipleTask_NPCBehavior::OnMoveCompleted);
			}
		}
	}

	AdvanceToNextNode();
}

void UMultipleTask_NPCBehavior::OnPlayAudioFinished()
{
	AdvanceToNextNode();
}

void UMultipleTask_NPCBehavior::OnAnimPlayed()
{
	AdvanceToNextNode();
}

void UMultipleTask_NPCBehavior::AdvanceToNextNode()
{
	if (!BehaviorSequence.IsValidIndex(CurrentBehaviorIndex))
	{
		return;
	}

	const FNPCBehaviorNode& CurrentNode = BehaviorSequence[CurrentBehaviorIndex];
	UWorld* World = GetWorld();

	if (CurrentNode.DelayBeforeNext > 0.f && World)
	{
		FTimerHandle Dummy;
		World->GetTimerManager().SetTimer(Dummy, this, &UMultipleTask_NPCBehavior::ExecuteNextNode,
			CurrentNode.DelayBeforeNext, false);
	}
	else
	{
		ExecuteNextNode();
	}
}
