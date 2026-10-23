#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AC_XSL.generated.h"


/*
 *动态挂载在像素流端控制器下的组件，适合用来远端RPC通信
 *
 */

UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class POLICEPLATFORM_API UAC_XSL : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UAC_XSL();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
