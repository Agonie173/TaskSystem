#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AC_PCPlayer.generated.h"

/**
 * PC玩家组件,负责PC玩家的逻辑功能
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class POLICEPLATFORM_API UAC_PCPlayer : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UAC_PCPlayer();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
