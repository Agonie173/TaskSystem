// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskAudioBroadcaster.h"

#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// Slot 蒙太奇路径使用的槽节点名（AnimBP 的 AnimGraph 输出链上须存在同名 Slot 节点）
	const FName DefaultSlotNodeName(TEXT("DefaultSlot"));
}

ATaskAudioBroadcaster::ATaskAudioBroadcaster()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// 无几何体的纯信令 Actor：AlwaysRelevant 保证所有客户端都建立通道、可收 Multicast
	bAlwaysRelevant = true;
}

int32 ATaskAudioBroadcaster::BroadcastPlayAudio(USoundBase* Sound, AActor* AttachActor, const FVector& Location,
	float Volume, float Pitch, bool bNeedAnim, UAnimSequence* AnimSequence)
{
	if (!Sound)
	{
		return INDEX_NONE;
	}

	const int32 Handle = NextPlayHandle++;

	// 广播时若无任何远程连接（任务被过早启动、等待机制未生效时），Multicast 会被静默丢弃。
	// 不能用 GetNumPlayers：LS 下 host 也计入，会在远程客户端未连接时给出假阴性
	if (UWorld* World = GetWorld())
	{
		bool bAnyRemoteClient = false;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (const APlayerController* PC = It->Get())
			{
				if (PC->GetNetConnection() != nullptr)
				{
					bAnyRemoteClient = true;
					break;
				}
			}
		}
		if (!World->IsNetMode(NM_Standalone) && !bAnyRemoteClient)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TaskAudioBroadcaster] No remote client connected at broadcast time, audio will not be heard on clients"));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[TaskAudioBroadcaster] Server broadcasting sound %s (handle=%d, attached=%d)"),
		*Sound->GetName(), Handle, AttachActor != nullptr);
	MulticastPlayAudio(Handle, Sound, AttachActor, Location, Volume, Pitch, bNeedAnim, AnimSequence);
	return Handle;
}

void ATaskAudioBroadcaster::BroadcastStopAudio(int32 Handle)
{
	if (Handle == INDEX_NONE)
	{
		return;
	}
	MulticastStopAudio(Handle);
}

void ATaskAudioBroadcaster::MulticastPlayAudio_Implementation(int32 Handle, USoundBase* Sound, AActor* AttachActor,
	const FVector& Location, float Volume, float Pitch, bool bNeedAnim, UAnimSequence* AnimSequence)
{
	// 仅客户端执行：服务器/单机的声音由任务自身的 AudioComponent 负责（避免双重播放）
	if (GetNetMode() != NM_Client)
	{
		return;
	}

	if (!Sound)
	{
		// RPC 到达但资产没解析出来（客户端缺该资源/路径不一致）时能立刻定位
		UE_LOG(LogTemp, Warning, TEXT("[TaskAudioBroadcaster] Client received play RPC but sound is null (handle=%d)"), Handle);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 惰性清理已销毁的组件（bAutoDestroy 播完自毁，TObjectPtr 失效）
	for (TMap<int32, TObjectPtr<UAudioComponent>>::TIterator It(ClientAudioByHandle); It; ++It)
	{
		if (!IsValid(It.Value().Get()))
		{
			It.RemoveCurrent();
		}
	}

	UAudioComponent* SpawnedAudio = nullptr;
	if (AttachActor && AttachActor->GetRootComponent())
	{
		// 附着播放：跟随目标移动、空间化；目标销毁时声音随之停止
		SpawnedAudio = UGameplayStatics::SpawnSoundAttached(
			Sound, AttachActor->GetRootComponent(), NAME_None, FVector::ZeroVector,
			EAttachLocation::SnapToTarget, /*bStopWhenAttachedToDestroyed=*/true,
			Volume, Pitch);
	}
	else
	{
		SpawnedAudio = UGameplayStatics::SpawnSoundAtLocation(
			World, Sound, Location, FRotator::ZeroRotator, Volume, Pitch);
	}

	if (!SpawnedAudio)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TaskAudioBroadcaster] Client failed to spawn sound %s (handle=%d)"),
			*Sound->GetName(), Handle);
		return;
	}

	ClientAudioByHandle.Add(Handle, SpawnedAudio);

	UE_LOG(LogTemp, Log, TEXT("[TaskAudioBroadcaster] Client playing sound %s (handle=%d, attached=%d, duration=%.2fs)"),
		*Sound->GetName(), Handle, AttachActor != nullptr, Sound->GetDuration());

	// 动画仅在附着模式下重建（与服务器 PlayAnim 相同的查找方式），走瞬态核心播完自动恢复
	if (bNeedAnim && AnimSequence && AttachActor)
	{
		if (const APawn* TargetPawn = Cast<APawn>(AttachActor))
		{
			if (USkeletalMeshComponent* SkelMesh = TargetPawn->FindComponentByClass<USkeletalMeshComponent>())
			{
				PlayTransientAnim(SkelMesh, AnimSequence);
			}
		}
	}
}

void ATaskAudioBroadcaster::MulticastStopAudio_Implementation(int32 Handle)
{
	// 仅客户端执行
	if (GetNetMode() != NM_Client)
	{
		return;
	}

	if (TObjectPtr<UAudioComponent>* Found = ClientAudioByHandle.Find(Handle))
	{
		if (IsValid(Found->Get()))
		{
			Found->Get()->Stop();
		}
		ClientAudioByHandle.Remove(Handle);

		UE_LOG(LogTemp, Log, TEXT("[TaskAudioBroadcaster] Client stopped audio (handle=%d)"), Handle);
	}
}

void ATaskAudioBroadcaster::BroadcastPlayAnim(AActor* TargetActor, UAnimSequenceBase* Anim)
{
	if (!TargetActor || !Anim)
	{
		return;
	}

	// 服务器本地瞬态播放（与客户端同一套核心：播完自动恢复原动画蓝图）
	if (USkeletalMeshComponent* Mesh = TargetActor->FindComponentByClass<USkeletalMeshComponent>())
	{
		PlayTransientAnim(Mesh, Anim);
	}

	UE_LOG(LogTemp, Log, TEXT("[TaskAudioBroadcaster] Server broadcasting anim %s on %s"),
		*Anim->GetName(), *TargetActor->GetName());
	MulticastPlayAnim(TargetActor, Anim);
}

void ATaskAudioBroadcaster::MulticastPlayAnim_Implementation(AActor* TargetActor, UAnimSequenceBase* Anim)
{
	// 仅客户端执行：服务器本地动画已由 BroadcastPlayAnim 驱动
	if (GetNetMode() != NM_Client)
	{
		return;
	}
	if (!Anim || !TargetActor)
	{
		// RPC 到达但对象参数没解析出来（客户端缺该资产/Actor）时能立刻定位
		UE_LOG(LogTemp, Warning, TEXT("[TaskAudioBroadcaster] Client received anim RPC but params null (anim=%d, actor=%d)"),
			Anim != nullptr, TargetActor != nullptr);
		return;
	}

	if (USkeletalMeshComponent* SkelMesh = TargetActor->FindComponentByClass<USkeletalMeshComponent>())
	{
		PlayTransientAnim(SkelMesh, Anim);
	}
}

void ATaskAudioBroadcaster::PlayTransientAnim(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Anim)
{
	if (!Mesh || !Anim)
	{
		return;
	}

	// Slot 蒙太奇优先（前提：AnimBP 的 AnimGraph 输出链上有 DefaultSlot 节点）：
	// 原生 BlendIn/BlendOut 平滑混回蓝图，播完/结束前自动过渡——无 A-pose、无定格，无需恢复定时器
	UAnimInstance* AnimInst = Mesh->GetAnimInstance();
	if (Mesh->GetAnimationMode() == EAnimationMode::AnimationBlueprint && AnimInst)
	{
		if (UAnimMontage* Montage = Cast<UAnimMontage>(Anim))
		{
			// 蒙太奇资产自带的 slot 段与 Blend 设置驱动过渡
			AnimInst->Montage_Play(Montage);
			UE_LOG(LogTemp, Log, TEXT("[TaskAudioBroadcaster] Slot montage %s"), *Anim->GetName());
			return;
		}
		if (Anim->IsA<UAnimSequence>())
		{
			// 序列动态包装为 DefaultSlot 蒙太奇；BlendOutTriggerTime 默认 -1 =
			// 动画结束前 BlendOutTime(0.25s) 开始混回蓝图（提前混合结束）
			AnimInst->PlaySlotAnimationAsDynamicMontage(Anim, DefaultSlotNodeName);
			UE_LOG(LogTemp, Log, TEXT("[TaskAudioBroadcaster] Slot dynamic montage %s"), *Anim->GetName());
			return;
		}
	}

	// 降级：SingleNode 路径（无动画蓝图实例时），保留恢复定时器机制（播完定格最后一帧，无蓝图可回）
	const TWeakObjectPtr<USkeletalMeshComponent> WeakMesh(Mesh);
	FTransientAnimRecord& Record = TransientAnims.FindOrAdd(WeakMesh);
	Record.Version++;

	// 播放前清掉旧的恢复定时器（版本号已保证安全，清句柄防表内悬挂）
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Record.RestoreTimerHandle);
	}

	Mesh->PlayAnimation(Anim, false);
	UE_LOG(LogTemp, Log, TEXT("[TaskAudioBroadcaster] Transient anim %s (single node, no AnimBP)"), *Anim->GetName());
}

void ATaskAudioBroadcaster::RestoreTransientAnim(TWeakObjectPtr<USkeletalMeshComponent> WeakMesh, int32 Version)
{
	// 版本号不匹配 = 该网格已开始播放更新的瞬态动画，本次恢复作废
	FTransientAnimRecord* Record = TransientAnims.Find(WeakMesh);
	if (!Record || Record->Version != Version)
	{
		return;
	}

	if (USkeletalMeshComponent* Mesh = WeakMesh.Get())
	{
		if (Record->OriginalClass)
		{
			Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			Mesh->SetAnimInstanceClass(Record->OriginalClass);
			UE_LOG(LogTemp, Log, TEXT("[TaskAudioBroadcaster] Restored anim blueprint on %s"),
				Mesh->GetOwner() ? *Mesh->GetOwner()->GetName() : TEXT("Unknown"));
		}
	}
	TransientAnims.Remove(WeakMesh);
}

int32 ATaskAudioBroadcaster::BroadcastSpawnActorOnClients(TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform,
	const TArray<int32>& TargetPlayerIds, bool bDestroyOnStepEnd)
{
	if (!ActorClass || TargetPlayerIds.IsEmpty())
	{
		return INDEX_NONE;
	}

	// 服务器权威句柄：EndTask 凭它广播删除
	const int32 SpawnHandle = NextSpawnHandle++;
	UE_LOG(LogTemp, Log, TEXT("[TaskAudioBroadcaster] Server dispatching local spawn %s to %d player(s) (handle=%d)"),
		*ActorClass->GetName(), TargetPlayerIds.Num(), SpawnHandle);
	MulticastSpawnLocalActor(SpawnHandle, ActorClass, SpawnTransform, TargetPlayerIds, bDestroyOnStepEnd);
	return SpawnHandle;
}

void ATaskAudioBroadcaster::MulticastSpawnLocalActor_Implementation(int32 SpawnHandle, TSubclassOf<AActor> ActorClass,
	FTransform SpawnTransform, const TArray<int32>& TargetPlayerIds, bool bDestroyOnStepEnd)
{
	// 仅客户端执行；客户端按自身 PlayerState 的 PlayerId 自判是否目标
	//（PlayerState 复制可靠；Actor Tags 不复制，客户端不能用标签自判）
	if (GetNetMode() != NM_Client)
	{
		return;
	}
	UWorld* World = GetWorld();
	const APlayerController* LocalPC = UGameplayStatics::GetPlayerController(World, 0);
	const APlayerState* LocalPS = LocalPC ? LocalPC->PlayerState : nullptr;
	if (!ActorClass || !World || !LocalPS || !TargetPlayerIds.Contains(LocalPS->GetPlayerId()))
	{
		return;
	}

	// 纯本地表现：强制关复制（防 CDO 复制属性带来的注册混乱），服务器无对应实例
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* LocalActor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, Params);
	if (!LocalActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TaskAudioBroadcaster] Client failed to spawn local actor %s"), *ActorClass->GetName());
		return;
	}
	if (LocalActor->GetIsReplicated())
	{
		LocalActor->SetReplicates(false);
	}

	if (bDestroyOnStepEnd)
	{
		ClientLocalActorsByHandle.Add(SpawnHandle, LocalActor);
	}
	UE_LOG(LogTemp, Log, TEXT("[TaskAudioBroadcaster] Client spawned local actor %s (handle=%d, destroy on step end: %d)"),
		*LocalActor->GetName(), SpawnHandle, bDestroyOnStepEnd);
}

void ATaskAudioBroadcaster::BroadcastDestroyLocalActor(int32 SpawnHandle)
{
	if (SpawnHandle == INDEX_NONE)
	{
		return;
	}
	MulticastDestroyLocalActor(SpawnHandle);
}

void ATaskAudioBroadcaster::MulticastDestroyLocalActor_Implementation(int32 SpawnHandle)
{
	if (GetNetMode() != NM_Client)
	{
		return;
	}

	// 未知句柄静默忽略（非目标客户端没有该映射）；bDestroyOnStepEnd=false 的保留项也不在映射里
	if (TWeakObjectPtr<AActor>* Found = ClientLocalActorsByHandle.Find(SpawnHandle))
	{
		if (AActor* LocalActor = Found->Get())
		{
			UE_LOG(LogTemp, Log, TEXT("[TaskAudioBroadcaster] Client destroying local actor %s (handle=%d)"),
				*LocalActor->GetName(), SpawnHandle);
			LocalActor->Destroy();
		}
		ClientLocalActorsByHandle.Remove(SpawnHandle);
	}
}

void ATaskAudioBroadcaster::BroadcastModifyActorTags(AActor* TargetActor, FName TagToModify, bool bAdd)
{
	if (!TargetActor || TagToModify.IsNone())
	{
		return;
	}
	MulticastModifyActorTags(TargetActor, TagToModify, bAdd);
}

void ATaskAudioBroadcaster::MulticastModifyActorTags_Implementation(AActor* TargetActor, FName TagToModify, bool bAdd)
{
	if (GetNetMode() != NM_Client)
	{
		return;
	}
	// Actor 引用解析失败：客户端尚未收到该复制 Actor（时序）或其未开复制
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TaskAudioBroadcaster] Client received tag RPC but actor unresolved (tag %s, add %d)"),
			*TagToModify.ToString(), bAdd);
		return;
	}

	if (bAdd)
	{
		if (!TargetActor->Tags.Contains(TagToModify))
		{
			TargetActor->Tags.Add(TagToModify);
		}
	}
	else
	{
		TargetActor->Tags.Remove(TagToModify);
	}
	UE_LOG(LogTemp, Log, TEXT("[TaskAudioBroadcaster] Client %s tag %s %s (now %d tags)"),
		bAdd ? TEXT("added") : TEXT("removed"), *TagToModify.ToString(),
		*TargetActor->GetName(), TargetActor->Tags.Num());
}
