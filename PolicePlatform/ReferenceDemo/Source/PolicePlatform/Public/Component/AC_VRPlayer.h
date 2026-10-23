#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AC_VRPlayer.generated.h"


/**
 * VR玩家组件,负责VR玩家的逻辑功能
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class POLICEPLATFORM_API UAC_VRPlayer : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UAC_VRPlayer();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
