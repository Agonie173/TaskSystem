#pragma once

#include "CoreMinimal.h"
#include "GamePlay/PPGameStateBase.h"
#include "Settings/Data.h"
#include "KJGameState.generated.h"

class APPPlayerBase;

//教练端观看玩家视角变化
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJLDLookWhoChange, APPPlayerBase*, LookWho);

/**
 * 课件游戏状态类
 */
UCLASS()
class POLICEPLATFORM_API AKJGameState : public APPGameStateBase
{
	GENERATED_BODY()
	
public:
	AKJGameState();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "警务|课件")
	TArray<FPlayerInfo> KJPlayers;
	
	//教练端当前正在观看哪个玩家的视角
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "警务|课件")
	APPPlayerBase* JLDLookWho = nullptr;
	
	UFUNCTION()
	void PlayerInfoListRemoveItem(const FString& Sn);
	
	//教练端观看玩家视角变化委托
	UPROPERTY(BlueprintAssignable, Category = "警务|课件")
	FOnJLDLookWhoChange OnJLDLookWhoChangeDelegate;
};