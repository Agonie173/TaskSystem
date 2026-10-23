#include "GamePlay/PPGameStateBase.h"
#include "Net/UnrealNetwork.h"

APPGameStateBase::APPGameStateBase()
{
}

void APPGameStateBase::BeginPlay()
{
	Super::BeginPlay();
}

void APPGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}