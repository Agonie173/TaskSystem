#pragma once

#include "CoreMinimal.h"
#include "GamePlay/PPPlayerStateBase.h"
#include "KJPlayerState.generated.h"

/**
 * 课件玩家状态类
 */
UCLASS()
class POLICEPLATFORM_API AKJPlayerState : public APPPlayerStateBase
{
	GENERATED_BODY()
	
public:
	AKJPlayerState();

	virtual void BeginPlay() override;
};