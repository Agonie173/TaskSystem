// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Settings/Data.h"
#include "GamePlay/PPPlayerControllerBase.h"
#include "Tools/OB_PlayerHelper.h"
#include "HallPlayerController.generated.h"

//收到任务实时事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskScoreReceived, FTaskResult, TaskScore);
//学员进入离开大厅
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerJoinCenterDS, bool, Join, FPlayerInfo, PlayerInfo);
//学员进入离开房间DS
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerJoinRoomDS, bool, Join, FPlayerInfo, PlayerInfo);
//学员状态变化
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerStatusChange, FPlayerStatus, PlayerStatus);
//房间DS启动关闭
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRoomDSChange, bool, Join, FRoomDSInfo, RoomDSInfo);
//像素流客户端正在看谁
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnXSLLookWho, FString, SN);


class UOB_PlayerHelper;

UCLASS()
class POLICEPLATFORM_API AHallPlayerController : public APPPlayerControllerBase
{
	GENERATED_BODY()

private:
	
	
public:
	
	AHallPlayerController();

	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	
	
	UPROPERTY()
	class AHallGameMode* GHallGameMode = nullptr;

	UPROPERTY()
	class AHallGameState* GHallGameState = nullptr;

	//RPC客户端安全开始
	UFUNCTION(Client, Reliable)
	void RPC_ClientStart(EProcessType LProcessType, const FPlayerInfo& LPlayerInfo);

#pragma region 教练端
	
	
	UPROPERTY(BlueprintReadOnly, Category = "警务|大厅控制器")
	TMap<FString, UOB_PlayerHelper*> PlayerHelperMap;
	
	//中心DS动态拉起房间DS
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "警务|大厅控制器")
	void RPC_ServerJLDStartRoomDS(int32 MapID, bool LIsXunLianMode, const TArray<FString>& SnArray);

	//教练端通知把某个房间DS里的学员踢出
	//暂时弃用
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "警务|大厅控制器")
	void RPC_ServerJLDNotifyKickRoomDSPlayer(const FString& LSn);

	//教练端通知看哪个玩家视口
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "警务|大厅控制器")
	void RPC_ServerJLDNotifyLookWho(const FString& LSn);
	
	//教练端通知中断所有训练
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "警务|大厅控制器")
	void RPC_ServerJLDNotifyCloseAll();
	
	//教练端RPC本地客户端实时事件播报 
	UFUNCTION(Client, Reliable)
	void RPC_ClientJLDNotifyTaskScore(FTaskResult LTaskResult);
	
	//教练端RPC本地客户端通知某个学员任务开始/结束
	UFUNCTION(Client, Reliable)
	void RPC_ClientJLDNotifyPlayerTaskStartEnd(bool LStart, const FString& LKey);

	//教练端RPC本地客户端通知某个学员暂停/恢复训练
	UFUNCTION(Client, Reliable)
	void RPC_ClientJLDNotifyPlayerPauseTrain(bool LPause, int32 LRoomID, const FString& LSn);

	//教练端Server RPC发送训练用时校准消息
	UFUNCTION(Server, Reliable)
	void RPC_ServerJLDSendCalibrationMessage(const FString& LSn, int32 LRoomID, int32 LTrainTime);
	
	//教练端RPC本地客户端通知像素流客户端正在看谁
	//暂时废弃了
	UFUNCTION(Client, Reliable)
	void RPC_ClientJLDNotifyLookWho(const FString& LSN);
	
	//实时事件任务分数接收委托
	UPROPERTY(BlueprintAssignable, Category = "警务|大厅控制器")
	FOnTaskScoreReceived OnTaskScoreReceivedDelegate;
	
	//玩家进入/离开中心DS
	UPROPERTY(BlueprintAssignable, Category = "警务|大厅控制器")
	FOnPlayerJoinCenterDS OnPlayerJoinCenterDSDelegate;
	
	//玩家进入/离开房间DS
	UPROPERTY(BlueprintAssignable, Category = "警务|大厅控制器")
	FOnPlayerJoinRoomDS OnPlayerJoinRoomDSDelegate;
	
	//学员状态变化
	UPROPERTY(BlueprintAssignable, Category = "警务|大厅控制器")
	FOnPlayerStatusChange OnPlayerStatusChangeDelegate;
	
	//房间DS启动关闭
	UPROPERTY(BlueprintAssignable, Category = "警务|大厅控制器")
	FOnRoomDSChange OnRoomDSChangeDelegate;
	
	//像素流客户端正在看谁
	UPROPERTY(BlueprintAssignable, Category = "警务|大厅控制器")
	FOnXSLLookWho OnXSLLookWhoDelegate;
	
	//教练端RPC本地客户端通知玩家进入/离开大厅DS
	UFUNCTION(Client, Reliable)
	void RPC_ClientJLDPlayerJoinCenterDS(bool Join, FPlayerInfo PlayerInfo);
	
	//教练端RPC本地客户端通知玩家进入/离开房间DS
	UFUNCTION(Client, Reliable)
	void RPC_ClientJLDPlayerJoinRoomDS(bool Join, FPlayerInfo PlayerInfo);
	
	//教练端RPC本地客户端通知学员状态变化
	UFUNCTION(Client, Unreliable)
	void RPC_ClientJLDPlayerStatusChange(FPlayerStatus PlayerStatus);
	
	//教练端RPC本地客户端通知房间DS启动关闭
	UFUNCTION(Client, Reliable)
	void RPC_ClientJLDRoomDSChange(bool Join, FRoomDSInfo RoomDSInfo);
	
	//服务器上通知玩家进入某个房间DS
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "警务|大厅控制器")
	void RPC_ServerJLDNotifyPlayerIntoRoomDS(int32 Port, int32 MapID, int32 RoomID, bool IsXunLianMode, const TArray<FString>& LocalSn);

	virtual void NotifyPlayerStatusChange(FPlayerStatus LPlayerStatus) override;

#pragma endregion

#pragma region 玩家客户端

	//玩家 RPC本地客户端设置玩家信息
	UFUNCTION(Client, Reliable)
	void PRC_ClientPlayerSetPlayerInfo(FPlayerInfo Info);


	//玩家 RPC本地客户端去连接房间DS
	UFUNCTION(Client, Reliable)
	void RPC_ClientPlayerIntoRoomDS(int32 RoomDSPort, int32 MapID, bool IsXunLianMode, int32 RoomID);

	
#pragma endregion
};
