// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GamePlay/PPGameModeBase.h"
#include "StartGameMode.generated.h"

/**
 * 游戏客户端默认开始地图
 */
UCLASS()
class POLICEPLATFORM_API AStartGameMode : public APPGameModeBase
{
	GENERATED_BODY()
	
	virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
};
