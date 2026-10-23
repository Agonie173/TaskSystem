// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/CourseNPCPathMovementComponent.h"
#include "Tools/CourseNPCPathActor.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "Interface/CourseLocalActorActionInterface.h"// 提供路线移动完成后的待机动作播放接口

// Sets default values for this component's properties
UCourseNPCPathMovementComponent::UCourseNPCPathMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// ...
}


// Called when the game starts
void UCourseNPCPathMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UCourseNPCPathMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsMoving || !IsValid(ActivePathActor))
	{
		return;
	}

	USplineComponent* RouteSpline = ActivePathActor->GetRouteSpline();

	if (!IsValid(RouteSpline))
	{
		UE_LOG(LogTemp, Error, TEXT("NPC路线移动已停止：路线Spline无效，NPC=%s"), *GetNameSafe(GetOwner()));
		StopCourseNPCPathMove();
		return;
	}

	const float SplineLength = RouteSpline->GetSplineLength();
	const float TravelDistance = ActiveMoveSpeed * FMath::Max(DeltaTime, 0.0f);

	if (bMoveReverse)
	{
		CurrentPathDistance -= TravelDistance;
	}
	else
	{
		CurrentPathDistance += TravelDistance;
	}

	const bool bReachedPathEnd = bMoveReverse ? CurrentPathDistance <= 0.0f : CurrentPathDistance >= SplineLength;
	CurrentPathDistance = FMath::Clamp(CurrentPathDistance, 0.0f, SplineLength);

	ApplyCurrentPathTransform();

	if (bReachedPathEnd)
	{
		FinishCourseNPCPathMove();
	}
}

bool UCourseNPCPathMovementComponent::StartCourseNPCPathMove(ACourseNPCPathActor* PathActor, float InMoveSpeed, bool bInMoveReverse, bool bInFaceMovementDirection, FName InActorId, FName InArrivalIdleActionId)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor))
	{
		UE_LOG(LogTemp, Error, TEXT("NPC路线移动启动失败：组件没有有效Owner"));
		return false;
	}

	if (!IsValid(PathActor))
	{
		UE_LOG(LogTemp, Error, TEXT("NPC路线移动启动失败：路线Actor无效，NPC=%s"), *OwnerActor->GetName());
		return false;
	}

	USplineComponent* RouteSpline = PathActor->GetRouteSpline();

	if (!IsValid(RouteSpline))
	{
		UE_LOG(LogTemp, Error, TEXT("NPC路线移动启动失败：路线Spline无效，NPC=%s，路线=%s"), *OwnerActor->GetName(), *PathActor->GetName());
		return false;
	}

	if (RouteSpline->GetSplineLength() <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Error, TEXT("NPC路线移动启动失败：路线长度为0，NPC=%s，路线=%s"), *OwnerActor->GetName(), *PathActor->GetName());
		return false;
	}

	if (InMoveSpeed <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Error, TEXT("NPC路线移动启动失败：移动速度必须大于0，NPC=%s，速度=%.2f"), *OwnerActor->GetName(), InMoveSpeed);
		return false;
	}

	StopCourseNPCPathMove();

	ActivePathActor = PathActor;
	ActiveMoveSpeed = InMoveSpeed;
	bMoveReverse = bInMoveReverse;
	bFaceMovementDirection = bInFaceMovementDirection;

	// 保存当前ActorId，供移动完成后的待机动作使用
	ActiveActorId = InActorId;

	// 保存到达终点后需要播放的待机动作ID
	ActiveArrivalIdleActionId = InArrivalIdleActionId;

	CurrentPathDistance = bMoveReverse ? RouteSpline->GetSplineLength() : 0.0f;
	bIsMoving = true;

	ApplyCurrentPathTransform();
	SetComponentTickEnabled(true);

	//UE_LOG(LogTemp, Warning, TEXT("NPC开始沿路线移动：NPC=%s，路线=%s，路线ID=%s，速度=%.2f，反向=%s"), *OwnerActor->GetName(), *PathActor->GetName(), *PathActor->RouteId.ToString(), ActiveMoveSpeed, bMoveReverse ? TEXT("是") : TEXT("否"));

	return true;
}

void UCourseNPCPathMovementComponent::StopCourseNPCPathMove()
{
	bIsMoving = false;
	ActivePathActor = nullptr;
	CurrentPathDistance = 0.0f;
	ActiveMoveSpeed = 0.0f;
	bMoveReverse = false;

	// 清除当前路线移动关联的课程ActorId
	ActiveActorId = NAME_None;

	// 清除当前路线移动结束后的待机动作ID
	ActiveArrivalIdleActionId = NAME_None;

	SetComponentTickEnabled(false);
}

bool UCourseNPCPathMovementComponent::IsCourseNPCPathMoving() const
{
	return bIsMoving;
}

void UCourseNPCPathMovementComponent::ApplyCurrentPathTransform()
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !IsValid(ActivePathActor))
	{
		return;
	}

	USplineComponent* RouteSpline = ActivePathActor->GetRouteSpline();

	if (!IsValid(RouteSpline))
	{
		return;
	}

	const FVector TargetLocation = RouteSpline->GetLocationAtDistanceAlongSpline(CurrentPathDistance, ESplineCoordinateSpace::World);

	if (!bFaceMovementDirection)
	{
		OwnerActor->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::None);
		return;
	}

	FVector MoveDirection = RouteSpline->GetDirectionAtDistanceAlongSpline(CurrentPathDistance, ESplineCoordinateSpace::World);

	if (bMoveReverse)
	{
		MoveDirection *= -1.0f;
	}

	if (MoveDirection.IsNearlyZero())
	{
		OwnerActor->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::None);
		return;
	}

	FRotator TargetRotation = MoveDirection.Rotation();
	TargetRotation.Pitch = 0.0f;
	TargetRotation.Roll = 0.0f;

	OwnerActor->SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::None);
}

void UCourseNPCPathMovementComponent::FinishCourseNPCPathMove()
{
	// 获取当前路线移动组件所属的NPC
	AActor* OwnerActor = GetOwner();

	// 在清理运行时变量前保存日志和待机动作需要的数据
	const FString OwnerName = GetNameSafe(OwnerActor);
	const FString PathName = GetNameSafe(ActivePathActor);
	const FName FinishedActorId = ActiveActorId;
	const FName FinishedArrivalIdleActionId = ActiveArrivalIdleActionId;

	// 标记当前路线移动已经完成
	bIsMoving = false;

	// 清理当前路线Actor和移动速度
	ActivePathActor = nullptr;
	ActiveMoveSpeed = 0.0f;

	// 清理当前动作关联信息
	ActiveActorId = NAME_None;
	ActiveArrivalIdleActionId = NAME_None;

	// 停止移动组件Tick，避免NPC到达终点后继续计算
	SetComponentTickEnabled(false);

	// 输出路线移动完成日志
	//UE_LOG(LogTemp, Warning, TEXT("NPC路线移动完成：NPC=%s，路线=%s"), *OwnerName, *PathName);

	// 配置了待机动作时通知NPC播放对应动作
	if (!FinishedArrivalIdleActionId.IsNone())
	{
		// NPC无效时无法恢复待机动作
		if (!IsValid(OwnerActor))
		{
			UE_LOG(LogTemp, Error, TEXT("NPC路线移动完成后播放待机动作失败：OwnerActor无效，动作=%s"), *FinishedArrivalIdleActionId.ToString());
		}
		// NPC必须实现现有课程本地Actor动作接口
		else if (!OwnerActor->GetClass()->ImplementsInterface(UCourseLocalActorActionInterface::StaticClass()))
		{
			UE_LOG(LogTemp, Error, TEXT("NPC路线移动完成后播放待机动作失败：NPC未实现动作接口，NPC=%s，动作=%s"), *OwnerName, *FinishedArrivalIdleActionId.ToString());
		}
		else
		{
			// 创建一条只用于恢复待机动画的Actor动作配置
			FCourseLocalActorActionDefinition IdleActionDefinition;

			// 保留当前NPC在课程配置中的ActorId
			IdleActionDefinition.ActorId = FinishedActorId;

			// 使用路线配置中的待机动作ID查找NPC的Action Animations
			IdleActionDefinition.ActionId = FinishedArrivalIdleActionId;

			// 待机动作需要播放动画
			IdleActionDefinition.bPlayAnimation = true;

			// 待机动画持续循环播放
			IdleActionDefinition.bLooping = true;

			// 当前动作本身已经是待机，不需要再次触发恢复待机
			IdleActionDefinition.bReturnToIdle = false;

			// 将待机动作发送给NPC现有动作接口
			ICourseLocalActorActionInterface::Execute_PlayCourseLocalActorAction(OwnerActor, IdleActionDefinition);

			// 输出待机动作开始日志
			//UE_LOG(LogTemp, Warning, TEXT("NPC路线移动完成后开始播放待机动作：NPC=%s，ActorId=%s，动作=%s"), *OwnerName, *FinishedActorId.ToString(), *FinishedArrivalIdleActionId.ToString());
		}
	}

	// 通知蓝图和其他监听者路线移动已经完成
	OnCourseNPCPathMoveFinished.Broadcast();
}

