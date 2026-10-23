#pragma once

#include "CoreMinimal.h"
#include "GamePlay/PPPlayerStateBase.h"
#include "HallPlayerState.generated.h"

/**
 * 大厅玩家状态类
 */
UCLASS()
class POLICEPLATFORM_API AHallPlayerState : public APPPlayerStateBase
{
	GENERATED_BODY()
	
public:
	AHallPlayerState();

	virtual void BeginPlay() override;
};