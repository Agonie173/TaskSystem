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
 * 嫌疑人 Pawn：
 * 骨骼网格体 + 碰撞盒 + Audio 组件，供后期添加动画 / 音频 / 嫌疑人移动。
 * 碰撞盒只对 Pawn 产生 Overlap（不阻挡），并把进出事件转发给 Sequencer，
 * 距离检测任务直接消费这些 Overlap 事件，无需 Tick。
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

	UFUNCTION()
	void HandleTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	UTaskSequencerComponent* GetSequencer() const;
};
