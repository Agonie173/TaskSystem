#include "GamePlay/PPGameModeBase.h"

APPGameModeBase::APPGameModeBase()
{
}

void APPGameModeBase::BeginPlay()
{
	Super::BeginPlay();
}

APlayerController* APPGameModeBase::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	return Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);
}

void APPGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
}

void APPGameModeBase::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
}

void APPGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	
	GWSSubsystem = GetGameInstance()->GetSubsystem<UWSSubsystem>();
}
