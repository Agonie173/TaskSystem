// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Engine/EngineTypes.h"
#include "SuspectPawn.generated.h"

class UAudioComponent;
class UBoxComponent;
class USkeletalMeshComponent;
class UPrimitiveComponent;
class UTaskSequencerComponent;

/**
 * 嫌疑人 Pawn（关卡预放置）：
 * 骨骼网格体 + 碰撞盒 + Audio 组件，供后期添加动画 / 音频 / 嫌疑人移动。
 *
 * 设计动机：距离/接近类任务（如搜查、盘问就位）只关心"谁进了嫌疑人周围
 * 的区域"，因此本 Pawn 自带一个只对 Pawn 产生 Overlap、不阻挡移动的
 * TriggerBox，把进出事件转发给 GameState 上的 Sequencer
 * （ForwardOverlapBegin/End），距离检测任务直接消费这些 Overlap 事件，
 * 无需任何 Tick 轮询。
 *
 * 转发约定：只在服务器转发（Overlap 在客户端也会触发，客户端转发会被
 * Sequencer 的权威检查忽略，白耗性能）；事件源统一是本 Pawn（this），
 * 进入者作为 OtherActor 传出。
 *
 * 框架侧注意：如果外层框架用关卡 TriggerVolume 之类的自定义触发体实现
 * 同类检测，也应照此模式把 Overlap 事件转发给 Sequencer 的
 * ForwardOverlapBegin/End，保证距离检测任务对事件来源无感知。
 */
UCLASS()
class POLICEPLATFORM_API ASuspectPawn : public APawn
{
	GENERATED_BODY()

public:
	ASuspectPawn();

	// 根组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootScene;

	// 骨骼网格体（美术后期放模型/动画）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* Mesh;

	// 距离检测碰撞盒（只 Overlap Pawn，不阻挡）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TriggerBox;

	// 音频组件（说话 / 动作音效）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAudioComponent* AudioComponent;

protected:
	virtual void BeginPlay() override;

	// TriggerBox 有 Pawn 进入时触发：服务器上转发给 Sequencer 的 ForwardOverlapBegin
	UFUNCTION()
	void HandleTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// TriggerBox 中 Pawn 离开时触发：服务器上转发给 Sequencer 的 ForwardOverlapEnd
	UFUNCTION()
	void HandleTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	// 从当前 World 的 GameState 上查找任务序列组件（找不到说明任务系统未启用）
	UTaskSequencerComponent* GetSequencer() const;
};
