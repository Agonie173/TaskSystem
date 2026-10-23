#include "Component/AC_VRPlayer.h"

UAC_VRPlayer::UAC_VRPlayer()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAC_VRPlayer::BeginPlay()
{
	Super::BeginPlay();
}

void UAC_VRPlayer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
