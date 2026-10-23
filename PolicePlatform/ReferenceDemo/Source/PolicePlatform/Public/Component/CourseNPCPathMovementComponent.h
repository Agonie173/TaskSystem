// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CourseNPCPathMovementComponent.generated.h"

class ACourseNPCPathActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCourseNPCPathMoveFinished);

UCLASS( ClassGroup=(PolicePlatform), meta=(BlueprintSpawnableComponent) )
class POLICEPLATFORM_API UCourseNPCPathMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCourseNPCPathMovementComponent();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "警务|NPC路线移动")
	bool StartCourseNPCPathMove(ACourseNPCPathActor* PathActor, float InMoveSpeed = 120.0f, bool bInMoveReverse = false, bool bInFaceMovementDirection = true, FName InActorId = NAME_None, FName InArrivalIdleActionId = NAME_None);
	
	UFUNCTION(BlueprintCallable, Category = "警务|NPC路线移动")
	void StopCourseNPCPathMove();

	UFUNCTION(BlueprintPure, Category = "警务|NPC路线移动")
	bool IsCourseNPCPathMoving() const;

	UPROPERTY(BlueprintAssignable, Category = "警务|NPC路线移动")
	FOnCourseNPCPathMoveFinished OnCourseNPCPathMoveFinished;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	void ApplyCurrentPathTransform();
	void FinishCourseNPCPathMove();

	UPROPERTY(Transient)
	TObjectPtr<ACourseNPCPathActor> ActivePathActor = nullptr;

	float CurrentPathDistance = 0.0f;
	float ActiveMoveSpeed = 0.0f;
	bool bMoveReverse = false;
	bool bFaceMovementDirection = true;
	bool bIsMoving = false;	

	// 保存当前移动Actor在课程配置中的ActorId
	FName ActiveActorId = NAME_None;

	// 保存到达终点后需要播放的待机动作ID
	FName ActiveArrivalIdleActionId = NAME_None;
};
