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
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "MultipleTask/TaskSequencerComponent.h"

void UMultipleTask_NPCBehavior::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	// 同一个任务实例可能被重复启动，先复位自身状态
	CleanupNodeState();
	ControlledNPCs.Reset();
	CurrentBehaviorIndex = -1;

	if (bUseExistingNPC)
	{
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
					UE_LOG(LogTemp, Log, TEXT("[NPCBehavior] Reusing existing NPC: %s"), *P->GetName());
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
				UE_LOG(LogTemp, Log, TEXT("[NPCBehavior] Spawned NPC %d/%d: %s at %s"),
					i + 1, NPCCount, *NewNPC->GetName(), *OffsetLoc.ToCompactString());
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
	CleanupNodeState();

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

		UE_LOG(LogTemp, Log, TEXT("[NPCBehavior] MoveTo target %s (acceptance %.1f)"),
			*MoveTarget.ToCompactString(), Node.AcceptanceRadius);

		UnbindMoveCompleted();
		PendingMoveCount = 0;

		for (APawn* NPC : ControlledNPCs)
		{
			if (!NPC) continue;
			AAIController* AICtrl = Cast<AAIController>(NPC->GetController());
			if (!AICtrl) continue;

			// 先移除再添加，避免同一节点被重复执行时委托叠加
			AICtrl->ReceiveMoveCompleted.RemoveDynamic(this, &UMultipleTask_NPCBehavior::OnMoveCompleted);
			AICtrl->ReceiveMoveCompleted.AddDynamic(this, &UMultipleTask_NPCBehavior::OnMoveCompleted);

			const EPathFollowingRequestResult::Type MoveResult =
				AICtrl->MoveToLocation(MoveTarget, Node.AcceptanceRadius);

			// 失败或本来就在目标点：不会再有完成回调，不计入等待数
			if (MoveResult != EPathFollowingRequestResult::RequestSuccessful)
			{
				AICtrl->ReceiveMoveCompleted.RemoveDynamic(this, &UMultipleTask_NPCBehavior::OnMoveCompleted);
				UE_LOG(LogTemp, Warning, TEXT("[NPCBehavior] %s MoveTo not started (result %d), skip waiting"),
					*NPC->GetName(), (int32)MoveResult);
				continue;
			}

			PendingMoveCount++;
			PendingMoveControllers.Add(AICtrl);
		}

		// 没有任何 NPC 真正在移动（没 AIController / 已到位），直接推进
		if (PendingMoveCount <= 0)
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
			World->GetTimerManager().SetTimer(NodeTimerHandle, this,
				&UMultipleTask_NPCBehavior::AdvanceToNextNode, Node.WaitDuration, false);
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
				UE_LOG(LogTemp, Log, TEXT("[NPCBehavior] Playing anim %s on %s"),
					*Node.AnimToPlay->GetName(), *NPC->GetName());
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
				// 先去重再绑定，避免多次播放时同一实例被绑定多次
				AudioComp->OnAudioFinished.RemoveDynamic(this, &UMultipleTask_NPCBehavior::OnPlayAudioFinished);
				AudioComp->OnAudioFinished.AddDynamic(this, &UMultipleTask_NPCBehavior::OnPlayAudioFinished);
				AudioComp->SetSound(Node.AudioToPlay);
				AudioComp->Play();
				CurrentAudioComponent = AudioComp;
				bStarted = true;
				UE_LOG(LogTemp, Log, TEXT("[NPCBehavior] Playing audio %s on %s"),
					*Node.AudioToPlay->GetName(), *NPC->GetName());
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
	if (Result != EPathFollowingResult::Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPCBehavior] MoveTo finished with result %d, continue anyway"), (int32)Result);
	}

	if (PendingMoveCount > 0)
	{
		--PendingMoveCount;
	}

	// 到位日志（含移动失败兜底的情况），便于核对多 NPC 的推进时机
	UE_LOG(LogTemp, Log, TEXT("[NPCBehavior] A NPC move finished, %d still moving"), PendingMoveCount);

	// 还有人没走到：等最后一个到达再推进（避免第一个到达就跳过整段移动）
	if (PendingMoveCount > 0)
	{
		return;
	}

	UnbindMoveCompleted();
	AdvanceToNextNode();
}

void UMultipleTask_NPCBehavior::UnbindMoveCompleted()
{
	for (const TWeakObjectPtr<AAIController>& WeakCtrl : PendingMoveControllers)
	{
		if (AAIController* AICtrl = WeakCtrl.Get())
		{
			AICtrl->ReceiveMoveCompleted.RemoveDynamic(this, &UMultipleTask_NPCBehavior::OnMoveCompleted);
		}
	}
	PendingMoveControllers.Reset();
	PendingMoveCount = 0;
}

void UMultipleTask_NPCBehavior::OnPlayAudioFinished()
{
	UE_LOG(LogTemp, Log, TEXT("[NPCBehavior] Audio finished, advance to next node"));

	if (CurrentAudioComponent)
	{
		CurrentAudioComponent->OnAudioFinished.RemoveDynamic(this, &UMultipleTask_NPCBehavior::OnPlayAudioFinished);
		CurrentAudioComponent = nullptr;
	}
	AdvanceToNextNode();
}

void UMultipleTask_NPCBehavior::OnAnimPlayed()
{
	AdvanceToNextNode();
}

void UMultipleTask_NPCBehavior::CleanupNodeState()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(NodeTimerHandle);
	}

	UnbindMoveCompleted();

	if (CurrentAudioComponent)
	{
		CurrentAudioComponent->OnAudioFinished.RemoveDynamic(this, &UMultipleTask_NPCBehavior::OnPlayAudioFinished);
		CurrentAudioComponent = nullptr;
	}
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
		World->GetTimerManager().SetTimer(NodeTimerHandle, this,
			&UMultipleTask_NPCBehavior::ExecuteNextNode, CurrentNode.DelayBeforeNext, false);
	}
	else
	{
		ExecuteNextNode();
	}
}
