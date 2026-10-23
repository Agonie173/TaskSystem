#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Settings/Data.h"
#include "PPPlayerStateBase.generated.h"

/**
 * 玩家状态基础类
 */
UCLASS()
class POLICEPLATFORM_API APPPlayerStateBase : public APlayerState
{
	GENERATED_BODY()
	
public:
	APPPlayerStateBase();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "警务|玩家状态")
	FPlayerInfo PlayerInfo;
};