// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WebSocketServer.h"
#include "GamePlay/KJGameMode.h"
#include "Settings/Data.h"

#include "WSSubsystem.generated.h"

class UInfoSubsystem;
class AHallGameMode;
class AHallGameState;
class UVaRestJsonObject;
class UBlueprintWebSocket;
class UVaRestSubsystem;


//踢房间玩家委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKillPlayerFromRoomDS, const FString&, Lsn);


/*
 *房间DS和中心DS websocket数据通信 子系统
 *因为房间DS和中心DS都是一个包体，所以得根据启动参数区分，从而走不同的逻辑
 */
UCLASS()
class POLICEPLATFORM_API UWSSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()


public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	
	//true是中心DS，false是房间DS，注意得延时下去获取才准确
	UPROPERTY(BlueprintReadOnly, Category="警务|ws")
	bool IsCenterDS = false;
	
	//第三方解析json数据的插件子系统
	UPROPERTY()
	UVaRestSubsystem* VaRestSubsystem;
	
	
#pragma region 中心DS
	
private:
	//中心DS创建一个，监听房间DS的连接
	UPROPERTY()
	UWebSocketServer* WebSocketServer = nullptr;

	//中心DS创建一个，去连接市局终端
	UPROPERTY()
	UBlueprintWebSocket* WebSocketClient_CenterDS = nullptr;
	
	UPROPERTY()
	AHallGameState* GHallGameState = nullptr;
	
	UPROPERTY()
	AHallGameMode* GHallGameMode = nullptr;
	
	UPROPERTY()
	UInfoSubsystem* GInfoSubsystem = nullptr;
	
	UFUNCTION()
	AHallGameState*  GetHallGameState();
	
	UFUNCTION()
	AHallGameMode* GetHallGameMode();
	
	UFUNCTION()
	UInfoSubsystem* GetInfoSubsystem();
	
	//WebSocket Server 回调函数
	UFUNCTION()
	void OnServerWebSocketOpenedCallback(UWebSocket* Socket);
	UFUNCTION()
	void OnServerWebSocketMessageCallback(UWebSocket* Socket, const FString& Message, EWebSocketOpCode OpCode);
	UFUNCTION()
	void OnServerWebSocketClosedCallback(UWebSocket* Socket, int32 Code, const FString& Reason);
	UFUNCTION()
	void OnServerWebSocketPingCallback(UWebSocket* Socket, const FString& Message, EWebSocketOpCode OpCode);
	UFUNCTION()
	void OnServerWebSocketPongCallback(UWebSocket* Socket, const FString& Message, EWebSocketOpCode OpCode);

	//WebSocket Server 监听回调函数
	UFUNCTION()
	void OnServerListeningCallback(bool bSuccess);

	//心跳检测定时器句柄
	FTimerHandle HeartbeatTimerHandle;
	
	//心跳检测定时器回调
	UFUNCTION()
	void OnHeartbeatTimerCallback();
	
	//玩家状态变化消息处理
	UFUNCTION()
	void WSJson_RoomDSPlayerStatusChange(UVaRestJsonObject* JsonObject);
	
	//房间DS拉起关闭变化消息处理
	UFUNCTION()
	void WSJson_RoomDSStartClose(UVaRestJsonObject* JsonObject);
	
	//房间DS玩家变动消息处理
	UFUNCTION()
	void WSJson_RoomDSPlayerChange(UVaRestJsonObject* JsonObject);
	
	//玩家任务分数数据解析
	UFUNCTION()
	void WSJson_TaskScore(UVaRestJsonObject* JsonObject);
	
	//玩家任务开始/结束数据解析
	UFUNCTION()
	void WSJson_TaskStartEnd(UVaRestJsonObject* JsonObject);
	
	//像素流正在看谁数据解析
	UFUNCTION()
	void WSJson_LookWho(UVaRestJsonObject* JsonObject);

	//暂停/恢复训练消息解析（房间DS发来的消息）
	UFUNCTION()
	void WSJson_PauseTrain(UVaRestJsonObject* JsonObject);
	
	
public:
	//走中心DS的ws流程
	UFUNCTION()
	void StartWS_CenterDS();
	
	//给房间DS发送数据
	UFUNCTION()
	void SendMsgToRoomDS(const FString& Msg);

	//踢玩家 暂时弃用了
	UFUNCTION()
	void WS_SendKillPlayerToRoomDS(const FString& Lsn);
	
	//发送教练端正在看哪个玩家到房间DS
	UFUNCTION()
	void WS_SendLookWhoToRoomDS(const FString& LSN);
	
	//发送教练端通知中断所有训练
	UFUNCTION()
	void WS_SendCloseAll();

	//发送训练用时校准消息到房间DS
	UFUNCTION()
	void WS_SendTrainTimeCalibrationToRoomDS(const FString& LSn, int32 LRoomID, int32 LTrainTime);

#pragma endregion
	
#pragma region 房间DS

private:
	//每个房间DS创建一个，去连接中心DS
	UPROPERTY()
	UBlueprintWebSocket* WebSocketClient = nullptr;

	//WebSocket Client 回调函数
	UFUNCTION()
	void OnClientConnectedCallback();
	UFUNCTION()
	void OnClientConnectionErrorCallback(const FString& Error);
	UFUNCTION()
	void OnClientClosedCallback(int64 StatusCode, const FString& Reason, bool bWasClean);
	UFUNCTION()
	void OnClientMessageCallback(const FString& Message);
	UFUNCTION()
	void OnClientRawMessageCallback(const TArray<uint8>& Data, int32 BytesReamining);
	UFUNCTION()
	void OnClientMessageSentCallback(const FString& Message);
	
	//课件GameMode
	UPROPERTY()
	AKJGameMode* GKJGameMode = nullptr;
	
	UFUNCTION()
	AKJGameMode* GetKJGameMode();
	
public:
	
	// 踢玩家广播
	UPROPERTY(BlueprintAssignable, Category = "警务")
	FOnKillPlayerFromRoomDS OnKillPlayerFromRoomDS;
	
	//走房间DS的ws流程
	UFUNCTION()
	void StartWS_RoomDS();

	//给中心DS发送数据
	UFUNCTION()
	void SendMsgToCenterDS(const FString& Msg);
	
	//发送房间DS中玩家进入/离开消息到中心DS
	UFUNCTION()
	void WS_SendRoomDSPlayersChangeToCenterDS(const FPlayerInfo& PlayerInfo, bool Join);
	
	//发送房间拉起/关闭消息到中心DS
	UFUNCTION()
	void WS_SendRoomDSStartCloseToCenterDS(bool Join);
	
	//发送玩家状态变化消息到中心DS，注意，只应该在学员状态有变化时才推送消息，不然浪费性能
	UFUNCTION()
	void WS_SendPlayerStatusChangeToCenterDS(const FPlayerStatus& PlayerStatus);
	
	//发送任务分数到中心DS
	UFUNCTION()
	void WS_SendTaskScoreToCenterDS(const FTaskResult& LTaskResult);
	
	//发送任务开始/结束到中心DS
	UFUNCTION()
	void WS_SendTaskStartEndToCenterDS(bool LbStart, const FString& LSN, int32 LRoomID);
	
	//发送像素流客户端正在看谁到中心DS
	//暂时废弃了,现在视口看谁由教练端决定，原来是点击像素流端UI决定的
	UFUNCTION()
	void WS_SendLookWhoToCenterDS(const FString& LSN);

	//发送暂停/恢复训练消息到中心DS
	UFUNCTION()
	void WS_SendPauseTrainToCenterDS(const FString& LSn, int32 LRoomID, bool LPause);

	//训练用时校准消息处理（中心DS发来的消息）
	UFUNCTION()
	void WSJson_TrainTimeCalibration(UVaRestJsonObject* JsonObject);
	
	//踢玩家消息处理
	UFUNCTION()
	void WSJson_KillPlayer(UVaRestJsonObject* JsonObject);
	
	//教练端正在看谁数据解析
	UFUNCTION()
	void WSJson_JLDLookWho(UVaRestJsonObject* JsonObject);
	
	//教练端中断所有训练
	UFUNCTION()
	void WSJson_CloseAll(UVaRestJsonObject* JsonObject);
	
	//房间DS退出并关闭进程
	UFUNCTION()
	void ExitRoomDS();
	
#pragma endregion
	
	
};
