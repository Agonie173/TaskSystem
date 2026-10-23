#include "GamePlay/KJGameState.h"
#include "Net/UnrealNetwork.h"

AKJGameState::AKJGameState()
{
}

void AKJGameState::BeginPlay()
{
	Super::BeginPlay();
}

void AKJGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AKJGameState, KJPlayers);
	DOREPLIFETIME(AKJGameState, JLDLookWho);
}

void AKJGameState::PlayerInfoListRemoveItem(const FString& Sn)
{
	for (int32 i = 0; i < KJPlayers.Num(); i++)
	{
		if (KJPlayers[i].SN == Sn)
		{
			KJPlayers.RemoveAt(i);
			break;
		}
	}
}