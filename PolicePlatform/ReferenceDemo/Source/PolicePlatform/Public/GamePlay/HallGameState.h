#pragma once

#include "CoreMinimal.h"
#include "GamePlay/PPGameStateBase.h"
#include "Settings/Data.h"
#include "HallGameState.generated.h"



//大厅玩家变更委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDaTingPlayerChange, const TArray<FPlayerInfo>&, DaTingPlayerInfoList);

//房间DS玩家变更委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomDSPlayerChange, const TArray<FPlayerInfo>&, RoomDSPlayerInfoList);

//房间DS玩家状态变更委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerStatusesChange, const TArray<FPlayerStatus>&, PlayerStatuses);

//房间DS信息变更委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomDSInfo, const TArray<FRoomDSInfo>&, AllRoomDS);

/**
 * 大厅
 */
UCLASS()
class POLICEPLATFORM_API AHallGameState : public APPGameStateBase
{
	GENERATED_BODY()
	
public:
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	//TODO 是否可以优化只复制到教练端，玩家端不复制？
	//大厅玩家
	UPROPERTY(ReplicatedUsing=OnRep_DaTingPlayerInfoList, BlueprintReadOnly, Category = "警务 | 游戏状态")
	TArray<FPlayerInfo> DaTingPlayers;
	
	
	//房间DS玩家
	UPROPERTY(ReplicatedUsing=OnRep_RoomDSPlayerInfoList, BlueprintReadOnly, Category = "警务 | 游戏状态")
	TArray<FPlayerInfo> RoomDSPlayers;
	
	//玩家状态
	UPROPERTY(ReplicatedUsing=OnRep_PlayerStatuses, BlueprintReadOnly, Category = "警务 | 游戏状态")
	TArray<FPlayerStatus> PlayerStatuses;
	
	//所有的房间DS
	UPROPERTY(ReplicatedUsing=OnRep_RoomDSInfo, BlueprintReadOnly, Category = "警务 | 游戏状态")
	TArray<FRoomDSInfo> AllRoomDS;
	
	// 复制通知回调函数
	UFUNCTION()
	void OnRep_DaTingPlayerInfoList();
	
	
	UFUNCTION()
	void OnRep_RoomDSPlayerInfoList();
	
	
	UFUNCTION()
	void OnRep_PlayerStatuses();
	
	
	UFUNCTION()
	void OnRep_RoomDSInfo();
	
	// 大厅玩家变更广播
	UPROPERTY(BlueprintAssignable, Category = "警务 | 游戏状态")
	FOnDaTingPlayerChange OnDaTingPlayerChange;

	// 房间DS玩家变更广播
	UPROPERTY(BlueprintAssignable, Category = "警务 | 游戏状态")
	FOnRoomDSPlayerChange OnRoomDSPlayerChange;
	
	// 房间DS玩家变更广播
	UPROPERTY(BlueprintAssignable, Category = "警务 | 游戏状态")
	FOnPlayerStatusesChange OnPlayerStatusesChange;
	
	// 房间DS拉起关闭变更广播
	UPROPERTY(BlueprintAssignable, Category = "警务 | 游戏状态")
	FOnRoomDSInfo OnRoomDSInfoCahnge;
	
	//从玩家信息数组里把指定玩家移除
	UFUNCTION()
	void PlayerInfoListRemoveItem(const FString& Sn, bool IsDaTingPlayer);
	
};
