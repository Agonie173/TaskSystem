// Fill out your copyright notice in the Description page of Project Settings.


#include "GamePlay/StartGameMode.h"
#include "GamePlay/PPPlayerControllerBase.h"

APlayerController* AStartGameMode::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal,
                                         const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	APlayerController* PC = Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);
	
	/*
	 
	APPPlayerControllerBase* LPC = Cast<APPPlayerControllerBase>(PC);
	
	//该地图必然是单机运行
	if (GetWorld()->GetNetMode() == NM_Standalone)
	{
		LPC->StandaloneInit();
	}
	
	*/
	
	return PC;
}
