// Fill out your copyright notice in the Description page of Project Settings.


#include "Tools/CourseNPCPathActor.h"
#include "Components/SplineComponent.h"

// Sets default values
ACourseNPCPathActor::ACourseNPCPathActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	RouteSpline = CreateDefaultSubobject<USplineComponent>(TEXT("RouteSpline"));
	SetRootComponent(RouteSpline);

	RouteSpline->SetClosedLoop(false);

}

// Called when the game starts or when spawned
void ACourseNPCPathActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ACourseNPCPathActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

USplineComponent* ACourseNPCPathActor::GetRouteSpline() const
{
	return RouteSpline;
}

