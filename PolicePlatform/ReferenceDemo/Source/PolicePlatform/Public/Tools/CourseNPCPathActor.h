// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CourseNPCPathActor.generated.h"

class USplineComponent;

UCLASS()
class POLICEPLATFORM_API ACourseNPCPathActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACourseNPCPathActor();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "警务|NPC路线")
	USplineComponent* GetRouteSpline() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "警务|NPC路线")
	FName RouteId = NAME_None;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "警务|NPC路线", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> RouteSpline = nullptr;
	

};
