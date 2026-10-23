#include "Component/AC_PCPlayer.h"

UAC_PCPlayer::UAC_PCPlayer()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAC_PCPlayer::BeginPlay()
{
	Super::BeginPlay();
}

void UAC_PCPlayer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
