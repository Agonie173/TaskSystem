// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GamePlay/PPPlayerControllerBase.h"
#include "StartPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class POLICEPLATFORM_API AStartPlayerController : public APPPlayerControllerBase
{
	GENERATED_BODY()
	
	virtual void OnPossess(APawn* InPawn) override;
	virtual void BeginPlay() override;
	
	void StartInit();
	
	
public:
	
	//单机模式打开课件地图
	UFUNCTION(BlueprintCallable, Category = "警务|控制器")
	void StartStandaloneKJMap(int32 LMapID, bool LIsXunLianMode);
};
