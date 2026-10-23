// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "PPGameInstance.generated.h"


class UInfoSubsystem;

UCLASS()
class POLICEPLATFORM_API UPPGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void OnStart() override;


private:
	UPROPERTY()
	UInfoSubsystem* GInfoSubsystem = nullptr;

	UFUNCTION()
	void RoomDSInit(const FString& CommandLine);
	
	UFUNCTION()
	void ViewPortInit(const FString& CommandLine);

	
public:
	//本地启动初始化，根据启动参数区分，从而进行不同的初始化操作
	UFUNCTION()
	void StartInit();
	
};
