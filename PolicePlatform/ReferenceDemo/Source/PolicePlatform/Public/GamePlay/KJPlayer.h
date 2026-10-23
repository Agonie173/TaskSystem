#pragma once

#include "CoreMinimal.h"
#include "GamePlay/PPPlayerBase.h"
#include "Component/AC_Task.h"
#include "KJPlayer.generated.h"

/**
 * 课件玩家类
 */
UCLASS()
class POLICEPLATFORM_API AKJPlayer : public APPPlayerBase
{
	GENERATED_BODY()
	
public:
	AKJPlayer();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	
};