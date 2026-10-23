#include "GamePlay/PPPlayerStateBase.h"
#include "Net/UnrealNetwork.h"

APPPlayerStateBase::APPPlayerStateBase()
{
}

void APPPlayerStateBase::BeginPlay()
{
	Super::BeginPlay();
}

void APPPlayerStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(APPPlayerStateBase, PlayerInfo);
}