// Fill out your copyright notice in the Description page of Project Settings.

#include "Network/WSSubsystem.h"
#include "WebSocketServer.h"
#include "BlueprintWebSocketWrapper.h"
#include "VaRestSubsystem.h"
#include "GamePlay/HallGameState.h"
#include "GamePlay/HallGameMode.h"
#include "GamePlay/HallPlayerController.h"
#include "GamePlay/KJGameMode.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Tools/InfoSubsystem.h"
#include "Settings/Data.h"


void UWSSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 获取 VaRest 子系统
	VaRestSubsystem = GEngine->GetEngineSubsystem<UVaRestSubsystem>();

	// 断言确保 VaRestSubsystem 不为空
	check(VaRestSubsystem);
}

void UWSSubsystem::Deinitialize()
{
	//中心DS退出时清理ws
	if (IsCenterDS)
	{
		if (WebSocketServer != nullptr)
		{
			//清理心跳检测定时器
			GetWorld()->GetTimerManager().ClearTimer(HeartbeatTimerHandle);

			//得保证所有的房间DS订阅了“管理”信息
			WebSocketServer->Publish("管理", "中心DS关闭了", EWebSocketOpCode::CLOSE);
			WebSocketServer->StopListening();
		}
		//如果连接了市局终端
		if (WebSocketClient != nullptr)
		{
			WebSocketClient->Close(1000, "某个分局的中心DS退出了");
		}
	}
	//房间DS退出时清理ws
	else
	{
		if (WebSocketClient != nullptr)
		{
			WebSocketClient->Close(1000, "房间DS退出了");
		}
	}

	Super::Deinitialize();
}

void UWSSubsystem::StartWS_CenterDS()
{
	IsCenterDS = true;

	WebSocketServer = UWebSocketServer::CreateWebSocketServer();
	if (!WebSocketServer)
	{
		UE_LOG(LogTemp, Error, TEXT("错误 - WebSocketServer创建失败"));
		return;
	}
	//必须大于等于8，说是底层决定的
	WebSocketServer->SetIdleTimeout(16);
	//关闭自动心跳检测
	WebSocketServer->SetSendPingsAutomatically(false);

	//绑定WebSocket Server回调
	WebSocketServer->OnWebSocketOpened.AddDynamic(this, &UWSSubsystem::OnServerWebSocketOpenedCallback);
	WebSocketServer->OnWebSocketMessage.AddDynamic(this, &UWSSubsystem::OnServerWebSocketMessageCallback);
	WebSocketServer->OnWebSocketClosed.AddDynamic(this, &UWSSubsystem::OnServerWebSocketClosedCallback);
	WebSocketServer->OnWebSocketPing.AddDynamic(this, &UWSSubsystem::OnServerWebSocketPingCallback);
	WebSocketServer->OnWebSocketPong.AddDynamic(this, &UWSSubsystem::OnServerWebSocketPongCallback);

	//启动WebSocket服务器监听
	WebSocketServer->Listen(
		TEXT("127.0.0.1"),
		TEXT("/*"),
		9000,
		FOnWebSocketServerListening::CreateUObject(this, &UWSSubsystem::OnServerListeningCallback)
	);
}

void UWSSubsystem::SendMsgToRoomDS(const FString& Msg)
{
	if (GIsEditor)
	{
		return;
	}
	
	if (!IsCenterDS || !WebSocketServer || Msg.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("不满足往房间DS发送消息的条件"));
		return;
	}

	WebSocketServer->Publish("管理", Msg, EWebSocketOpCode::TEXT);
	UE_LOG(LogTemp, Log, TEXT("往房间DS发送了消息：%s"), *Msg);
}

void UWSSubsystem::WS_SendKillPlayerToRoomDS(const FString& Lsn)
{
	if (Lsn.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("错误，踢玩家时传入的玩家编号为空"));
	}
	// 使用 VaRest 创建 JSON 对象
	UVaRestJsonObject* LJsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
	check(LJsonObject);

	// 设置字段
	LJsonObject->SetStringField(TEXT("Type"), TEXT("KillPlayer"));
	LJsonObject->SetStringField(TEXT("SN"), Lsn);

	SendMsgToRoomDS(LJsonObject->EncodeJsonToSingleString());
}

void UWSSubsystem::WS_SendLookWhoToRoomDS(const FString& LSN)
{
	if (LSN.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("错误，教练端给房间DS发送正在看哪个玩家的消息时玩家编号为空"));
	}
	
	UVaRestJsonObject* LJsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
	check(LJsonObject);

	LJsonObject->SetStringField(TEXT("Type"), TEXT("JLDLookWho"));
	LJsonObject->SetStringField(TEXT("SN"), LSN);
	
	SendMsgToRoomDS(LJsonObject->EncodeJsonToSingleString());
}

void UWSSubsystem::WS_SendCloseAll()
{
	UVaRestJsonObject* LJsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
	check(LJsonObject);

	LJsonObject->SetStringField(TEXT("Type"), TEXT("CloseAll"));
	
	SendMsgToRoomDS(LJsonObject->EncodeJsonToSingleString());
}

void UWSSubsystem::WS_SendTrainTimeCalibrationToRoomDS(const FString& LSn, int32 LRoomID, int32 LTrainTime)
{
	UVaRestJsonObject* LJsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
	check(LJsonObject);

	LJsonObject->SetStringField(TEXT("Type"), TEXT("TrainTimeCalibration"));
	LJsonObject->SetStringField(TEXT("SN"), LSn);
	LJsonObject->SetIntegerField(TEXT("RoomID"), LRoomID);
	LJsonObject->SetIntegerField(TEXT("TrainTime"), LTrainTime);

	SendMsgToRoomDS(LJsonObject->EncodeJsonToSingleString());
}

void UWSSubsystem::StartWS_RoomDS()
{
	IsCenterDS = false;

	WebSocketClient = UBlueprintWebSocket::CreateWebSocket();
	if (!WebSocketClient)
	{
		UE_LOG(LogTemp, Error, TEXT("错误 - WebSocketClient创建失败"));
		return;
	}

	//绑定WebSocket Client回调
	WebSocketClient->OnConnectedEvent.AddDynamic(this, &UWSSubsystem::OnClientConnectedCallback);
	WebSocketClient->OnConnectionErrorEvent.AddDynamic(this, &UWSSubsystem::OnClientConnectionErrorCallback);
	WebSocketClient->OnCloseEvent.AddDynamic(this, &UWSSubsystem::OnClientClosedCallback);
	WebSocketClient->OnMessageEvent.AddDynamic(this, &UWSSubsystem::OnClientMessageCallback);
	//WebSocketClient->OnRawMessageEvent.AddDynamic(this, &UWSSubsystem::OnClientRawMessageCallback);
	WebSocketClient->OnMessageSentEvent.AddDynamic(this, &UWSSubsystem::OnClientMessageSentCallback);

	//TODO待考虑连接失败的情况处理
	//去连接中心DS，即使两台电脑的环境也是把所有DS运行在同一台电脑上，所以这里写死使用127.0.0.1是对的
	WebSocketClient->Connect("ws://127.0.0.1:9000", "ws");
}

void UWSSubsystem::SendMsgToCenterDS(const FString& Msg)
{
	
	if (GIsEditor)
	{
		return;
	}
	
	if (IsCenterDS || !WebSocketClient || Msg.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("不满足往中DS发送消息的条件"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("往中心DS发送了消息 %s"), *Msg);
	WebSocketClient->SendMessage(Msg);
}

void UWSSubsystem::WS_SendRoomDSPlayersChangeToCenterDS(const FPlayerInfo& PlayerInfo, bool Join)
{
	// 使用 VaRest 创建 JSON 对象
	UVaRestJsonObject* LJsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
	check(LJsonObject);

	// 设置字段
	LJsonObject->SetStringField(TEXT("Type"), TEXT("PlayersChange"));
	LJsonObject->SetStringField(TEXT("SN"), PlayerInfo.SN);
	LJsonObject->SetStringField(TEXT("PlayerName"), PlayerInfo.PlayerName);
	LJsonObject->SetIntegerField(TEXT("MapID"), PlayerInfo.MapID);
	LJsonObject->SetIntegerField(TEXT("RoomID"), PlayerInfo.RoomID);
	
	LJsonObject->SetBoolField(TEXT("Join"), Join);
	
	// 序列化为字符串
	SendMsgToCenterDS(LJsonObject->EncodeJsonToSingleString());
	
}

void UWSSubsystem::WS_SendRoomDSStartCloseToCenterDS(bool Join)
{
	UInfoSubsystem* LInfoSubsystem = GetInfoSubsystem();
	
	// 使用 VaRest 创建 JSON 对象
	UVaRestJsonObject* LJsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
	check(LJsonObject);

	
	// 设置字段
	LJsonObject->SetStringField(TEXT("Type"), TEXT("RoomInfo"));
	LJsonObject->SetIntegerField(TEXT("MapID"), LInfoSubsystem->RoomDSInfo.MapID);
	LJsonObject->SetIntegerField(TEXT("RoomID"), LInfoSubsystem->RoomDSInfo.RoomID);
	LJsonObject->SetIntegerField(TEXT("Port"), LInfoSubsystem->RoomDSInfo.Port);
	LJsonObject->SetBoolField(TEXT("Join"), Join);
	
	SendMsgToCenterDS(LJsonObject->EncodeJsonToSingleString());
}

void UWSSubsystem::WS_SendPlayerStatusChangeToCenterDS(const FPlayerStatus& PlayerStatus)
{
	
	// 使用 VaRest 创建 JSON 对象
	UVaRestJsonObject* LJsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
	check(LJsonObject);

	// 设置字段
	LJsonObject->SetStringField(TEXT("Type"), TEXT("PlayerStatusChange"));
	LJsonObject->SetStringField(TEXT("SN"), PlayerStatus.SN);
	LJsonObject->SetIntegerField(TEXT("HeadsetBattery"), PlayerStatus.HeadsetBattery);
	LJsonObject->SetIntegerField(TEXT("TouchBattery"), PlayerStatus.TouchBattery);
	
	SendMsgToCenterDS(LJsonObject->EncodeJsonToSingleString());
}

void UWSSubsystem::WS_SendTaskScoreToCenterDS(const FTaskResult& LTaskResult)
{
	UVaRestJsonObject* LJsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
	check(LJsonObject);

	LJsonObject->SetStringField(TEXT("Type"), TEXT("TaskScore"));
	LJsonObject->SetStringField(TEXT("SN"), LTaskResult.SN);
	LJsonObject->SetIntegerField(TEXT("TaskIndex"), LTaskResult.TaskIndex);
	LJsonObject->SetIntegerField(TEXT("MapID"), LTaskResult.MapID);
	LJsonObject->SetIntegerField(TEXT("RoomID"), LTaskResult.RoomID);
	LJsonObject->SetStringField(TEXT("TaskName"), LTaskResult.TaskName);
	
	FString ResultStr = TEXT("None");
	switch (LTaskResult.TaskResult)
	{
	case ETaskResult::Succeed:
		ResultStr = TEXT("Succeed");
		break;
	case ETaskResult::Fail:
		ResultStr = TEXT("Fail");
		break;
	case ETaskResult::TimeOut:
		ResultStr = TEXT("TimeOut");
		break;
	case ETaskResult::IsGetOutOfLine:
		ResultStr = TEXT("IsGetOutOfLine");
		break;
	default:
		break;
	}
	
	LJsonObject->SetStringField(TEXT("TaskResult"), ResultStr);
	LJsonObject->SetStringField(TEXT("RecordText"), LTaskResult.RecordText);
	LJsonObject->SetStringField(TEXT("YYSpeak"), LTaskResult.YYSpeak);
	LJsonObject->SetIntegerField(TEXT("Score"), LTaskResult.Score);
	LJsonObject->SetStringArrayField(TEXT("KeyWords"), LTaskResult.Keywords);
	
	SendMsgToCenterDS(LJsonObject->EncodeJsonToSingleString());
}

void UWSSubsystem::WS_SendTaskStartEndToCenterDS(bool LbStart, const FString& LSN, int32 LRoomID)
{
	UVaRestJsonObject* LJsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
	check(LJsonObject);

	LJsonObject->SetStringField(TEXT("Type"), TEXT("TaskStartEnd"));
	LJsonObject->SetBoolField(TEXT("Start"), LbStart);
	LJsonObject->SetStringField(TEXT("SN"), LSN);
	LJsonObject->SetIntegerField(TEXT("RoomID"), LRoomID);
	
	SendMsgToCenterDS(LJsonObject->EncodeJsonToSingleString());
}

void UWSSubsystem::WS_SendLookWhoToCenterDS(const FString& LSN)
{
	UVaRestJsonObject* LJsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
	check(LJsonObject);

	LJsonObject->SetStringField(TEXT("Type"), TEXT("LookWho"));
	LJsonObject->SetStringField(TEXT("SN"), LSN);
	
	SendMsgToCenterDS(LJsonObject->EncodeJsonToSingleString());
}

void UWSSubsystem::WS_SendPauseTrainToCenterDS(const FString& LSn, int32 LRoomID, bool LPause)
{
	UVaRestJsonObject* LJsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
	check(LJsonObject);

	LJsonObject->SetStringField(TEXT("Type"), TEXT("PauseTrain"));
	LJsonObject->SetStringField(TEXT("SN"), LSn);
	LJsonObject->SetIntegerField(TEXT("RoomID"), LRoomID);
	LJsonObject->SetBoolField(TEXT("Pause"), LPause);

	SendMsgToCenterDS(LJsonObject->EncodeJsonToSingleString());
}

AHallGameState* UWSSubsystem::GetHallGameState()
{
	if (GHallGameState == nullptr)
	{
		GHallGameState = Cast<AHallGameState>(GetWorld()->GetGameState());
	}
	
	return GHallGameState;
	
}

AHallGameMode* UWSSubsystem::GetHallGameMode()
{
	if (GHallGameMode == nullptr)
	{
		GHallGameMode = Cast<AHallGameMode>(GetWorld()->GetAuthGameMode());
	}
	
	return GHallGameMode;
	
}

UInfoSubsystem* UWSSubsystem::GetInfoSubsystem()
{
	if (GInfoSubsystem == nullptr)
	{
		GInfoSubsystem = GetGameInstance()->GetSubsystem<UInfoSubsystem>();
	}
	
	return GInfoSubsystem;
}

//WebSocket Server 回调函数实现
void UWSSubsystem::OnServerWebSocketOpenedCallback(UWebSocket* Socket)
{
	//有房间DS连接进来
	if (Socket)
	{
		UE_LOG(LogTemp, Log, TEXT("%s 房间DS连接进来了"), *Socket->GetName());
		//所有房间DS都需要订阅管理频道
		Socket->Subscribe("管理");
	}
}

void UWSSubsystem::OnServerWebSocketMessageCallback(UWebSocket* Socket, const FString& Message, EWebSocketOpCode OpCode)
{
	UE_LOG(LogTemp, Log, TEXT("%s - 收到房间DS发来的消息:%s"), *Socket->GetName(), *Message);


	// 使用 VaRest 创建 JSON 对象
	UVaRestJsonObject* JsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
	check(JsonObject);
	JsonObject->DecodeJson(Message);

	// 获取消息类型
	FString LType = JsonObject->GetStringField(TEXT("Type"));

	
	// 根据信息类型做JSON的解析
	if (LType == TEXT("PlayersChange"))
	{
		//玩家进入/离开房间DS
		WSJson_RoomDSPlayerChange(JsonObject);
	}
	else if (LType == TEXT("PlayerStatusChange"))
	{
		//房间DS里的玩家状态变化
		WSJson_RoomDSPlayerStatusChange(JsonObject);
	}
	else if (LType == TEXT("RoomInfo"))
	{
		//房间DS拉起/关闭
		WSJson_RoomDSStartClose(JsonObject);
	}
	else if (LType == TEXT("TaskScore"))
	{
		//玩家单个任务完成情况上报
		WSJson_TaskScore(JsonObject);
	}
	else if (LType == TEXT("TaskStartEnd"))
	{
		//某个玩家训练开始/结束
		WSJson_TaskStartEnd(JsonObject);
	}
	else if (LType == TEXT("PauseTrain"))
	{
		//某个玩家暂停/恢复训练
		WSJson_PauseTrain(JsonObject);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("未知的房间DS发来的消息类型: %s"), *LType);
	}

}

void UWSSubsystem::OnServerWebSocketClosedCallback(UWebSocket* Socket, int32 Code, const FString& Reason)
{
	//有房间DS断开了
	if (Socket)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s 房间DS断开了！！！ - Code:%d - 信息:%s"), *Socket->GetName(), Code, *Reason);
		//1000代表房间DS调用了close
		if (1000 == Code)
		{
		}
		else
		{
			//其它情况，比如心跳检测失败了
			UE_LOG(LogTemp, Warning, TEXT("%s 房间DS心跳检测失败了！！！"), *Socket->GetName());
		}
	}
}

void UWSSubsystem::OnServerWebSocketPingCallback(UWebSocket* Socket, const FString& Message, EWebSocketOpCode OpCode)
{
	//TODO: 收到Ping消息后的处理逻辑
}

void UWSSubsystem::OnServerWebSocketPongCallback(UWebSocket* Socket, const FString& Message, EWebSocketOpCode OpCode)
{
	//TODO 后期可关闭打印，心跳打印太多妨碍调试
	/*if (Socket)
	{
		UE_LOG(LogTemp, Log, TEXT("%s 收到房间DS的pong"), *Socket->GetName());
	}*/
}

//WebSocket Server 监听回调函数实现
void UWSSubsystem::OnServerListeningCallback(bool bSuccess)
{
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("WebSocket服务器启动监听成功！"));

		//启动心跳检测定时器，每5秒执行一次
		GetWorld()->GetTimerManager().SetTimer(HeartbeatTimerHandle, this, &UWSSubsystem::OnHeartbeatTimerCallback,
		                                       5.0f, true);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("WebSocket服务器启动监听失败！"));
	}
}

//心跳检测定时器回调
void UWSSubsystem::OnHeartbeatTimerCallback()
{
	if (WebSocketServer)
	{
		//给所有房间DS发送心跳检测消息，所有房间DS需要订阅管理频道
		WebSocketServer->Publish("管理", "心跳检测", EWebSocketOpCode::PING);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("错误 - WebSocketServer为空，心跳检测"));
		GetWorld()->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
	}
}

void UWSSubsystem::WSJson_RoomDSPlayerStatusChange(UVaRestJsonObject* JsonObject)
{
	check(JsonObject);
	
	FPlayerStatus LPlayerStatus;
	
	LPlayerStatus.SN = JsonObject->GetStringField(TEXT("SN"));
	LPlayerStatus.HeadsetBattery = JsonObject->GetIntegerField(TEXT("HeadsetBattery"));
	LPlayerStatus.TouchBattery = JsonObject->GetIntegerField(TEXT("TouchBattery"));
	
	AHallGameState* LHallGameState = GetHallGameState();
	AHallGameMode* LHallGameMode = GetHallGameMode();
	
	if (LHallGameMode->JLD_PC)
	{
		LHallGameMode->JLD_PC->RPC_ClientJLDPlayerStatusChange(LPlayerStatus);
	}
	
	
	//TODO 待优化
	for (int i = 0; i < LHallGameState->PlayerStatuses.Num(); ++i)
	{
		if (LHallGameState->PlayerStatuses[i].SN == LPlayerStatus.SN)
		{
			LHallGameState->PlayerStatuses.RemoveAt(i);
			break;
		}
	}
	
	LHallGameState->PlayerStatuses.Add(LPlayerStatus);
	
	
}

void UWSSubsystem::WSJson_RoomDSStartClose(UVaRestJsonObject* JsonObject)
{
	check(JsonObject);
	
	// 创建玩家信息结构体
	FRoomDSInfo LRoomDSInfo;
	
	LRoomDSInfo.MapID = JsonObject->GetIntegerField(TEXT("MapID"));
	LRoomDSInfo.RoomID = JsonObject->GetIntegerField(TEXT("RoomID"));
	LRoomDSInfo.Port = JsonObject->GetIntegerField(TEXT("Port"));
	bool Join = JsonObject->GetBoolField(TEXT("Join"));
	
	
	if (Join)
	{
		GetHallGameState()->AllRoomDS.Add(LRoomDSInfo);
		
		
		// 根据RoomID查找待连接的玩家SN数组
		TArray<FString>* WaitingPlayers = GetHallGameMode()->RoomWaitingPlayers.Find(LRoomDSInfo.RoomID);
		bool LMode = *GetHallGameMode()->RoomWaitingPlayersMode.Find(LRoomDSInfo.RoomID);
		
		if (WaitingPlayers && WaitingPlayers->Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("房间DS连接成功，通知%d个玩家进入房间DS，RoomID=%d"), WaitingPlayers->Num(), LRoomDSInfo.RoomID);
				
			// 通知玩家进入房间DS
			GetHallGameMode()->NotifyPlayerIntoRoomDS(LRoomDSInfo.Port, LRoomDSInfo.MapID, LRoomDSInfo.RoomID, LMode, *WaitingPlayers);
			GetHallGameMode()->JLD_PC->RPC_ClientJLDRoomDSChange(true, LRoomDSInfo);
			
			// 从字典中移除，避免重复通知
			GetHallGameMode()->RoomWaitingPlayers.Remove(LRoomDSInfo.RoomID);
			GetHallGameMode()->RoomWaitingPlayersMode.Remove(LRoomDSInfo.RoomID);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("错误，房间DS连接成功，但没有待连接的玩家，RoomID=%d"), LRoomDSInfo.RoomID);
		}
	}
	else
	{
		GetHallGameMode()->JLD_PC->RPC_ClientJLDRoomDSChange(false, LRoomDSInfo);
		
		for (int i = 0; i < GetHallGameState()->AllRoomDS.Num(); ++i)
		{
			if (GetHallGameState()->AllRoomDS[i].RoomID == LRoomDSInfo.RoomID)
			{
				GetHallGameState()->AllRoomDS.RemoveAt(i);
				//有房间DS关闭，回收利用端口
				GetInfoSubsystem()->RecyclingPort(LRoomDSInfo.Port);
				break;
			}
		}
	}
}

void UWSSubsystem::WSJson_RoomDSPlayerChange(UVaRestJsonObject* JsonObject)
{
	check(JsonObject);

	// 创建玩家信息结构体
	FPlayerInfo LPlayerInfo;
	
	LPlayerInfo.SN = JsonObject->GetStringField(TEXT("SN"));
	LPlayerInfo.PlayerName = JsonObject->GetStringField(TEXT("PlayerName"));
	LPlayerInfo.MapID = JsonObject->GetIntegerField(TEXT("MapID"));
	LPlayerInfo.RoomID = JsonObject->GetIntegerField(TEXT("RoomID"));
	
	AHallGameState* LHallGameState = GetHallGameState();

	AHallGameMode* LHallGameMode = GetHallGameMode();
	
	
	if (JsonObject->GetBoolField(TEXT("Join")))
	{
		LHallGameState->RoomDSPlayers.Add(LPlayerInfo);
		LHallGameMode->JLD_PC->RPC_ClientJLDPlayerJoinRoomDS(true, LPlayerInfo);
		UE_LOG(LogTemp, Log, TEXT("玩家进入房间DS 玩家名字：%s"), *LPlayerInfo.PlayerName);
	}
	else
	{
		LHallGameState->PlayerInfoListRemoveItem(LPlayerInfo.SN,false);
		LHallGameMode->JLD_PC->RPC_ClientJLDPlayerJoinRoomDS(false, LPlayerInfo);
		UE_LOG(LogTemp, Log, TEXT("玩家离开房间DS 玩家名字：%s"), *LPlayerInfo.PlayerName);
	}
}

void UWSSubsystem::WSJson_TaskScore(UVaRestJsonObject* JsonObject)
{
	check(JsonObject);
	
	/*
	考核时间以大厅PC时间为准
	FDateTime Now = FDateTime::Now();
	FString::Printf(TEXT("%d-%d-%d-%d:%d"), Now.GetYear(), Now.GetMonth(), Now.GetDay(), Now.GetHour(), Now.GetMinute());
	 */
	
	
	FTaskResult LTaskResult;
	
	LTaskResult.SN = JsonObject->GetStringField(TEXT("SN"));
	LTaskResult.TaskIndex = JsonObject->GetIntegerField(TEXT("TaskIndex"));
	LTaskResult.MapID = JsonObject->GetIntegerField(TEXT("MapID"));
	LTaskResult.RoomID = JsonObject->GetIntegerField(TEXT("RoomID"));
	LTaskResult.TaskName = JsonObject->GetStringField(TEXT("TaskName"));
	
	FString ResultStr = JsonObject->GetStringField(TEXT("TaskResult"));
	if (ResultStr == TEXT("Succeed"))
	{
		LTaskResult.TaskResult = ETaskResult::Succeed;
	}
	else if (ResultStr == TEXT("Fail"))
	{
		LTaskResult.TaskResult = ETaskResult::Fail;
	}
	else if (ResultStr == TEXT("TimeOut"))
	{
		LTaskResult.TaskResult = ETaskResult::TimeOut;
	}
	else if (ResultStr == TEXT("IsGetOutOfLine"))
	{
		LTaskResult.TaskResult = ETaskResult::IsGetOutOfLine;
	}
	else
	{
		LTaskResult.TaskResult = ETaskResult::None;
	}
	
	LTaskResult.RecordText = JsonObject->GetStringField(TEXT("RecordText"));
	LTaskResult.YYSpeak = JsonObject->GetStringField(TEXT("YYSpeak"));
	LTaskResult.Score = JsonObject->GetIntegerField(TEXT("Score"));
	
	LTaskResult.Keywords = JsonObject->GetStringArrayField(TEXT("KeyWords"));
	
	// 教练端实时事件播报
	if (GetHallGameMode()->JLD_PC)
	{
		GetHallGameMode()->JLD_PC->RPC_ClientJLDNotifyTaskScore(LTaskResult);
	}
}

void UWSSubsystem::WSJson_TaskStartEnd(UVaRestJsonObject* JsonObject)
{
	check(JsonObject);
	
	bool LbTaskStart = JsonObject->GetBoolField(TEXT("Start"));
	FString LKey = FString::Printf(TEXT("%d-%s"), JsonObject->GetIntegerField(TEXT("RoomID")), *JsonObject->GetStringField(TEXT("SN")));
	
	// 教练端通知某个玩家任务开始/结束
	if (GetHallGameMode()->JLD_PC)
	{
		GetHallGameMode()->JLD_PC->RPC_ClientJLDNotifyPlayerTaskStartEnd(LbTaskStart, LKey);
	}
}

void UWSSubsystem::WSJson_LookWho(UVaRestJsonObject* JsonObject)
{
	check(JsonObject);
	
	// 教练端通知某个玩家任务开始/结束
	if (GetHallGameMode()->JLD_PC)
	{
		GetHallGameMode()->JLD_PC->RPC_ClientJLDNotifyLookWho(JsonObject->GetStringField(TEXT("SN")));
	}
}

void UWSSubsystem::WSJson_PauseTrain(UVaRestJsonObject* JsonObject)
{
	check(JsonObject);

	const FString LSn = JsonObject->GetStringField(TEXT("SN"));
	const int32 LRoomID = JsonObject->GetIntegerField(TEXT("RoomID"));
	const bool LPause = JsonObject->GetBoolField(TEXT("Pause"));

	// 教练端通知某个玩家暂停/恢复训练
	if (GetHallGameMode()->JLD_PC)
	{
		GetHallGameMode()->JLD_PC->RPC_ClientJLDNotifyPlayerPauseTrain(LPause, LRoomID, LSn);
	}
}

//WebSocket Client 回调函数实现
void UWSSubsystem::OnClientConnectedCallback()
{
	UE_LOG(LogTemp, Log, TEXT("WebSocket客户端连接成功"));
	
	//延时后通知中心DS，有房间DS被拉起了
	//TODO 延时测试
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
	{
		WS_SendRoomDSStartCloseToCenterDS(true);
	}, 0.02f, false);
}

void UWSSubsystem::OnClientConnectionErrorCallback(const FString& Error)
{
	UE_LOG(LogTemp, Error, TEXT("WebSocket客户端连接失败: %s"), *Error);
}

void UWSSubsystem::OnClientClosedCallback(int64 StatusCode, const FString& Reason, bool bWasClean)
{
	UE_LOG(LogTemp, Error, TEXT("WebSocket客户端连接关闭 - 状态码: %d, 原因: %s, 是否干净关闭: %s"),
	       StatusCode, *Reason, bWasClean ? TEXT("是") : TEXT("否"));
	
	UE_LOG(LogTemp, Log, TEXT("房间DS即将关闭"));
	FGenericPlatformMisc::RequestExit(false);
}

void UWSSubsystem::OnClientMessageCallback(const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("收到中心DS发来的消息:%s"), *Message);

	// 使用 VaRest 创建 JSON 对象
	UVaRestJsonObject* JsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
	check(JsonObject);
	
	JsonObject->DecodeJson(Message);

	// 获取消息类型
	FString LType = JsonObject->GetStringField(TEXT("Type"));

	// 根据信息类型做JSON的解析
	if (LType == TEXT("JLDLookWho"))
	{
		//教练端观看玩家视角变化
		WSJson_JLDLookWho(JsonObject);
	}
	else if (LType == TEXT("CloseAll"))
	{
		//中断所有训练
		WSJson_CloseAll(JsonObject);
	}
	else if (LType == TEXT("TrainTimeCalibration"))
	{
		//训练用时校准
		WSJson_TrainTimeCalibration(JsonObject);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("未知的中心DS发来的消息类型: %s"), *LType);
	}
	
	
}

void UWSSubsystem::OnClientRawMessageCallback(const TArray<uint8>& Data, int32 BytesReamining)
{
	//测试发现收到服务器发来的文本数据时也会触发该回调，暂且不绑定该回调
	//UE_LOG(LogTemp, Log, TEXT("WebSocket客户端收到二进制消息，长度: %d"), Data.Num());
}

void UWSSubsystem::OnClientMessageSentCallback(const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("WebSocket客户端发送消息成功: %s"), *Message);
}

AKJGameMode* UWSSubsystem::GetKJGameMode()
{
	if (GKJGameMode == nullptr)
	{
		GKJGameMode = Cast<AKJGameMode>(GetWorld()->GetAuthGameMode());
	}
	
	return GKJGameMode;
	
}

void UWSSubsystem::WSJson_KillPlayer(UVaRestJsonObject* JsonObject)
{
	check(JsonObject);
	
	FString LSN = JsonObject->GetStringField(TEXT("SN"));
	UE_LOG(LogTemp, Log, TEXT("教练端通知踢出玩家 玩家编号：%s"), *LSN);
	
	OnKillPlayerFromRoomDS.Broadcast(LSN);
}

void UWSSubsystem::WSJson_JLDLookWho(UVaRestJsonObject* JsonObject)
{
	check(JsonObject);
	
	FString LSN = JsonObject->GetStringField(TEXT("SN"));
	UE_LOG(LogTemp, Log, TEXT("教练端通知看玩家视角 玩家编号：%s"), *LSN);
	
	
	// 教练端通知观看某个玩家视角
	GetKJGameMode()->JLDLookWhoChange(LSN);
	   
}

void UWSSubsystem::WSJson_CloseAll(UVaRestJsonObject* JsonObject)
{
	UE_LOG(LogActor, Log, TEXT("收到中断所有训练的通知"));
	
	GetKJGameMode()->KillAllPlayer();
}

void UWSSubsystem::WSJson_TrainTimeCalibration(UVaRestJsonObject* JsonObject)
{
	check(JsonObject);

	const FString LSn = JsonObject->GetStringField(TEXT("SN"));
	const int32 LRoomID = JsonObject->GetIntegerField(TEXT("RoomID"));
	const int32 LTrainTime = JsonObject->GetIntegerField(TEXT("TrainTime"));

	// 校准训练用时
	AKJGameMode* LGameMode = GetKJGameMode();
	if (LGameMode)
	{
		LGameMode->CalibrateTrainTime(LSn, LRoomID, LTrainTime);
	}
}

void UWSSubsystem::ExitRoomDS()
{
	if (IsCenterDS)
	{
		UE_LOG(LogTemp, Error, TEXT("错误 - 当前是中心DS，不能调用ExitRoomDS"));
		return;
	}
	
	//发消息通知中心DS要退出了
	WS_SendRoomDSStartCloseToCenterDS(false);
	
	FTimerHandle LTimerHandle1;
	//延时下再关闭ws，不然会导致中心DS收不到消息
	GetWorld()->GetTimerManager().SetTimer(
		LTimerHandle1,    
		[this]()
		{
			//关闭WebSocket连接
			if (WebSocketClient != nullptr)
			{
				WebSocketClient->Close(1000, TEXT("房间DS主动退出"));
			}
			
		},
		1.0f,
		false
	);
}

