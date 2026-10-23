#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "PPGameStateBase.generated.h"

/**
 * 游戏状态基础类
 */
UCLASS()
class POLICEPLATFORM_API APPGameStateBase : public AGameState
{
	GENERATED_BODY()
	
public:
	APPGameStateBase();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};