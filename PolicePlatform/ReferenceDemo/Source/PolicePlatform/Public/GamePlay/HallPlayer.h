#pragma once

#include "CoreMinimal.h"
#include "GamePlay/PPPlayerBase.h"
#include "HallPlayer.generated.h"

/**
 * 大厅玩家类
 */
UCLASS()
class POLICEPLATFORM_API AHallPlayer : public APPPlayerBase
{
	GENERATED_BODY()
	
public:
	AHallPlayer();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
};