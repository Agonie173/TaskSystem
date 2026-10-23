// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TaskAudioBroadcaster.generated.h"

class USoundBase;
class UAnimSequence;
class UAnimSequenceBase;
class UAnimInstance;
class UAudioComponent;
class USkeletalMeshComponent;

/**
 * 音频广播通道：解决"任务音频只在服务器响"的网络问题。
 *
 * 背景：UE 的音频播放（AudioComponent::Play / SpawnSoundAtLocation）只在执行端本地出声，
 * 组件复制不会驱动客户端播放。任务系统里的音频任务（PlayAudio 等）只存在于服务器，
 * 因此 DS 下客户端听不到任何任务音频。
 *
 * 方案：服务器生成一个 bAlwaysRelevant 的轻量复制 Actor，把"播放/停止"作为
 * NetMulticast Reliable RPC 广播给所有客户端；客户端在本地重建播放：
 *   - AttachActor 有效：SpawnSoundAttached 到该 Actor 根组件（跟随移动、空间化）
 *   - 否则：SpawnSoundAtLocation 在指定世界坐标播放
 * 动画同理：客户端在目标 Pawn 的骨骼网格上重建 PlayAnimation。
 *
 * 与服务器侧任务流的关系：服务器上 PlayAudio 任务仍用自己的 AudioComponent
 * 驱动 OnAudioFinished 推进清单（Listen 服务器也因此可闻）；本 Actor 只负责
 * 让客户端出声，广播调用后服务器本地不会重复播放（RPC 体仅在客户端执行）。
 *
 * 停止语义：BroadcastPlayAudio 返回服务器自增句柄，任务在 EndTask 时凭句柄
 * 广播 MulticastStopAudio，客户端按句柄精准停止对应组件（bAutoDestroy 自动销毁）。
 * 音频属瞬态表现，玩家中途加入不补播（无状态复制），属预期行为。
 */
UCLASS()
class POLICEPLATFORM_API ATaskAudioBroadcaster : public AActor
{
	GENERATED_BODY()

public:
	ATaskAudioBroadcaster();

	/**
	 * 服务器调用：广播一段音频（附带可选动画），返回本次播放句柄（供 EndTask 停止用）。
	 * AttachActor 有效时忽略 Location，附着其根组件播放；否则在世界坐标 Location 播放。
	 */
	int32 BroadcastPlayAudio(USoundBase* Sound, AActor* AttachActor, const FVector& Location,
		float Volume, float Pitch, bool bNeedAnim, UAnimSequence* AnimSequence);

	/** 服务器调用：广播停止指定句柄对应的客户端音频（未知句柄静默忽略） */
	void BroadcastStopAudio(int32 Handle);

	/** 服务器调用：广播在目标 Actor 的骨骼网格上播放一段动画（与音频共用本通道）。
	 *  服务器本地与客户端都走"瞬态播放"：动画时长后自动恢复原动画蓝图 */
	void BroadcastPlayAnim(AActor* TargetActor, UAnimSequenceBase* Anim);

	/** 服务器调用：广播在指定玩家的客户端上本地生成 Actor（仅目标玩家可见）。
	 *  返回生成句柄（供 EndTask 广播删除）；TargetPlayerIds 为匹配玩家的 PlayerId 列表 */
	int32 BroadcastSpawnActorOnClients(TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform,
		const TArray<int32>& TargetPlayerIds, bool bDestroyOnStepEnd);

	/** 服务器调用：广播删除指定句柄对应的客户端本地 Actor（未知句柄静默忽略） */
	void BroadcastDestroyLocalActor(int32 SpawnHandle);

	/** 服务器调用：广播修改复制 Actor 的标签（Tags 不复制，客户端按 Actor 引用解析同一实例后本地同步）。
	 *  bAdd = true 添加 / false 删除 */
	void BroadcastModifyActorTags(AActor* TargetActor, FName TagToModify, bool bAdd);

private:
	/** 瞬态动画记录：播放前抓取的原动画蓝图类 + 播放版本号（防旧恢复覆盖新播放） */
	struct FTransientAnimRecord
	{
		TSubclassOf<UAnimInstance> OriginalClass;
		int32 Version = 0;
		FTimerHandle RestoreTimerHandle;
	};

	/**
	 * 瞬态动画播放核心（服务器本地与客户端 Multicast 体共用）：
	 * 记录原动画蓝图 → 切 SingleNode 播放 → 动画时长后恢复蓝图（无蓝图则停在最后一帧）。
	 * 同一网格的新播放会使旧恢复作废（版本号比对），多段/循环播放不会被中途打断回蓝图。
	 */
	void PlayTransientAnim(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Anim);

	/** 恢复原动画蓝图（版本号不匹配时忽略） */
	void RestoreTransientAnim(TWeakObjectPtr<USkeletalMeshComponent> WeakMesh, int32 Version);

private:
	// 下一个播放句柄（服务器权威自增，客户端不生成）
	int32 NextPlayHandle = 0;

	// 下一个本地生成句柄（服务器权威自增，客户端按它记录/删除本地 Actor）
	int32 NextSpawnHandle = 0;

	// 瞬态动画记录表（服务器与客户端各自维护，键为骨骼网格弱引用）
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FTransientAnimRecord> TransientAnims;

	// 客户端侧：句柄 -> 播放中的音频组件（供远程停止；失效项在每次播放前惰性清理）
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UAudioComponent>> ClientAudioByHandle;

	// 广播播放（RPC 体仅在客户端执行：服务器/单机的声音由任务自身的 AudioComponent 负责）
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayAudio(int32 Handle, USoundBase* Sound, AActor* AttachActor, const FVector& Location,
		float Volume, float Pitch, bool bNeedAnim, UAnimSequence* AnimSequence);
	void MulticastPlayAudio_Implementation(int32 Handle, USoundBase* Sound, AActor* AttachActor, const FVector& Location,
		float Volume, float Pitch, bool bNeedAnim, UAnimSequence* AnimSequence);

	// 广播停止（RPC 体仅在客户端执行）
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopAudio(int32 Handle);
	void MulticastStopAudio_Implementation(int32 Handle);

	// 广播动画播放（RPC 体仅在客户端执行；一次性播放自然结束，无需停止广播）
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayAnim(AActor* TargetActor, UAnimSequenceBase* Anim);
	void MulticastPlayAnim_Implementation(AActor* TargetActor, UAnimSequenceBase* Anim);

	// 广播客户端本地生成 Actor（RPC 体仅在客户端执行：客户端按自身 PlayerId 自判是否目标）
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSpawnLocalActor(int32 SpawnHandle, TSubclassOf<AActor> ActorClass, FTransform SpawnTransform,
		const TArray<int32>& TargetPlayerIds, bool bDestroyOnStepEnd);
	void MulticastSpawnLocalActor_Implementation(int32 SpawnHandle, TSubclassOf<AActor> ActorClass, FTransform SpawnTransform,
		const TArray<int32>& TargetPlayerIds, bool bDestroyOnStepEnd);

	// 广播删除客户端本地 Actor（RPC 体仅在客户端执行）
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDestroyLocalActor(int32 SpawnHandle);
	void MulticastDestroyLocalActor_Implementation(int32 SpawnHandle);

	// 广播复制 Actor 标签修改（RPC 体仅在客户端执行；Actor 引用参数由引擎跨机自动解析）
	UFUNCTION(NetMulticast, Reliable)
	void MulticastModifyActorTags(AActor* TargetActor, FName TagToModify, bool bAdd);
	void MulticastModifyActorTags_Implementation(AActor* TargetActor, FName TagToModify, bool bAdd);

	// 客户端侧：生成句柄 -> 本地 Actor（供远程删除；bDestroyOnStepEnd=false 的保留项不删除）
	UPROPERTY(Transient)
	TMap<int32, TWeakObjectPtr<AActor>> ClientLocalActorsByHandle;
};
