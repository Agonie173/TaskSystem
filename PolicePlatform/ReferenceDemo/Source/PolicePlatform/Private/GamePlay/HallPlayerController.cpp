// Fill out your copyright notice in the Description page of Project Settings.


#include "GamePlay/HallPlayerController.h"
#include "BFL/BFL_Common.h"
#include "GamePlay/HallGameMode.h"
#include "GamePlay/HallGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Network/WSSubsystem.h"
#include "Settings/Data.h"
#include "String/Join.h"
#include "Tools/InfoSubsystem.h"

AHallPlayerController::AHallPlayerController()
{
	
}

void AHallPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
}

void AHallPlayerController::BeginPlay()
{
	if (HasAuthority())
	{
		// 缓存大厅游戏模式引用
		GHallGameMode = Cast<AHallGameMode>(GetWorld()->GetAuthGameMode());
	}
	
	GHallGameState = Cast<AHallGameState>(GetWorld()->GetGameState());
	
	Super::BeginPlay();
	
}

void AHallPlayerController::RPC_ServerJLDNotifyCloseAll_Implementation()
{
	GetWSSubsystem()->WS_SendCloseAll();
}

void AHallPlayerController::RPC_ServerJLDNotifyLookWho_Implementation(const FString& LSn)
{
	GetWSSubsystem()->WS_SendLookWhoToRoomDS(LSn);
}

void AHallPlayerController::RPC_ClientJLDNotifyLookWho_Implementation(const FString& LSN)
{
	/*
	OnXSLLookWhoDelegate.Broadcast(LSN);
	*/
}

void AHallPlayerController::RPC_ClientJLDNotifyPlayerTaskStartEnd_Implementation(bool LStart, const FString& LKey)
{
	//某个学员开始/结束任务时，找到对应学员的object，开启计时/结束计时
	UOB_PlayerHelper** PlayerHelperPtr = PlayerHelperMap.Find(LKey);
	if (PlayerHelperPtr && *PlayerHelperPtr)
	{
		if (LStart)
		{
			(*PlayerHelperPtr)->StartTaskTimer();
		}
		else
		{
			(*PlayerHelperPtr)->StopTaskTimer();
		}
		
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("错误 任务开始/结束消息到达但未找到对应PlayerHelper，Key=%s"), *LKey);
	}
}

void AHallPlayerController::RPC_ClientJLDNotifyPlayerPauseTrain_Implementation(bool LPause, int32 LRoomID, const FString& LSn)
{
	FString Key = FString::Printf(TEXT("%d-%s"), LRoomID, *LSn);

	UOB_PlayerHelper** PlayerHelperPtr = PlayerHelperMap.Find(Key);
	if (PlayerHelperPtr && *PlayerHelperPtr)
	{
		if (LPause)
		{
			(*PlayerHelperPtr)->PauseTrain();
		}
		else
		{
			(*PlayerHelperPtr)->ResumeTrain();
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("错误 暂停/恢复训练消息到达但未找到对应PlayerHelper，Key=%s"), *Key);
	}
}

void AHallPlayerController::RPC_ServerJLDSendCalibrationMessage_Implementation(const FString& LSn, int32 LRoomID, int32 LTrainTime)
{
	GetWSSubsystem()->WS_SendTrainTimeCalibrationToRoomDS(LSn, LRoomID, LTrainTime);
}

void AHallPlayerController::RPC_ClientJLDNotifyTaskScore_Implementation(FTaskResult LTaskResult)
{
	FString Key = FString::Printf(TEXT("%d-%s"), LTaskResult.RoomID, *LTaskResult.SN);
	
	UOB_PlayerHelper** PlayerHelperPtr = PlayerHelperMap.Find(Key);
	if (PlayerHelperPtr && *PlayerHelperPtr)
	{
		UE_LOG(LogTemp, Error, TEXT("任务结果加到TaskScoreArray数组里，Key=%s"), *Key);
		(*PlayerHelperPtr)->TaskScoreArray.Add(LTaskResult);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("错误 任务成绩消息到达但未找到对应PlayerHelper，Key=%s"), *Key);
	}
	
	OnTaskScoreReceivedDelegate.Broadcast(LTaskResult);
}

void AHallPlayerController::RPC_ClientStart_Implementation(EProcessType LProcessType, const FPlayerInfo& LPlayerInfo)
{
	
	//TODO 该函数是不是可以提到父类里
	if (LProcessType == EProcessType::VR_XYD)
	{
		//立马上报一次
		//VR_ReportHeadsetBattery(true);
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_VR_ReportHeadsetBattery, [this]()
		{
			if (IsValid(this))
			{
				VR_ReportBattery(true);
			}
			
		}, 5.0f, true);
	}
	
	ClientStart_BIE(LProcessType, LPlayerInfo);
}



void AHallPlayerController::NotifyPlayerStatusChange(FPlayerStatus LPlayerStatus)
{
	Super::NotifyPlayerStatusChange(LPlayerStatus);
	
	if (GHallGameMode && GHallGameMode->JLD_PC)
	{
		UE_LOG(LogTemp, Log, TEXT("调用教练端控制器的RPC_ClientJLDPlayerStatusChange"));
		GHallGameMode->JLD_PC->RPC_ClientJLDPlayerStatusChange(LPlayerStatus);
	}
	
}


void AHallPlayerController::RPC_ServerJLDNotifyKickRoomDSPlayer_Implementation(const FString& LSn)
{
	GetWSSubsystem()->WS_SendKillPlayerToRoomDS(LSn);
}

void AHallPlayerController::RPC_ClientJLDRoomDSChange_Implementation(bool Join, FRoomDSInfo RoomDSInfo)
{
	//有新开的房间DS就要相应拉取一个像素流客户端
	if (Join)
	{
		FString LFilePath = TEXT("");
		UBFL_Common::GenerateBatchScript_XSLClient(RoomDSInfo.Port, RoomDSInfo.MapID, RoomDSInfo.RoomID, LFilePath);
		UBFL_Common::ExecuteBatchScript(LFilePath);
		UE_LOG(LogTemp, Log, TEXT("启动一个像素流客户端 端口 %d 地图ID %d 房间ID %d"), RoomDSInfo.Port, RoomDSInfo.MapID, RoomDSInfo.RoomID);
	}
	
	FString LType = Join ? TEXT("启动") : TEXT("关闭");
	UE_LOG(LogTemp, Log, TEXT("%s 了一个房间DS 课件%d - 房间%d"), *LType, RoomDSInfo.MapID, RoomDSInfo.RoomID);
	OnRoomDSChangeDelegate.Broadcast(Join, RoomDSInfo);
}

void AHallPlayerController::RPC_ClientJLDPlayerStatusChange_Implementation(FPlayerStatus PlayerStatus)
{
	UE_LOG(LogTemp, Log, TEXT("学员 %s 状态变化"), *PlayerStatus.PlayerName);
	OnPlayerStatusChangeDelegate.Broadcast(PlayerStatus);
}


void AHallPlayerController::RPC_ClientJLDPlayerJoinRoomDS_Implementation(bool Join, FPlayerInfo PlayerInfo)
{
	FString LType = Join ? TEXT("进入") : TEXT("离开");
	UE_LOG(LogTemp, Log, TEXT("学员 %s %s房间DS 课件%d-房间%d"), *PlayerInfo.PlayerName, *LType, PlayerInfo.MapID, PlayerInfo.RoomID);
	
	FString Key = FString::Printf(TEXT("%d-%s"), PlayerInfo.RoomID, *PlayerInfo.SN);
	
	if (Join)
	{
		//学员进入房间DS时新建一个object对象，在里面计时和任务成绩信息
		UOB_PlayerHelper* PlayerHelper = NewObject<UOB_PlayerHelper>(this);
		PlayerHelper->Init(PlayerInfo.SN, PlayerInfo.RoomID, this);
		PlayerHelperMap.Add(Key, PlayerHelper);
	}
	else
	{
		/*
		*学员离开房间DS时把对应的从字典里移除，因为学员在结束时可以点击重新体验再次加入同一个RoomID的房间DS训练
		*防止上一次训练的任务成绩干扰
		*/
		UOB_PlayerHelper** PlayerHelperPtr = PlayerHelperMap.Find(Key);
		if (PlayerHelperPtr && *PlayerHelperPtr)
		{
			(*PlayerHelperPtr)->ConditionalBeginDestroy();
			*PlayerHelperPtr = nullptr;
		}
		PlayerHelperMap.Remove(Key);
	}
	
	OnPlayerJoinRoomDSDelegate.Broadcast(Join, PlayerInfo);
}

void AHallPlayerController::RPC_ClientJLDPlayerJoinCenterDS_Implementation(bool Join, FPlayerInfo PlayerInfo)
{
	//这里延时是为了保证让教练端UI有时间先绑定
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this, Join, PlayerInfo]()
	{
		if (!IsValid(this)) return;
		FString LType = Join ? TEXT("进入") : TEXT("离开");
		UE_LOG(LogTemp, Log, TEXT("学员 %s %s大厅"), *PlayerInfo.PlayerName, *LType);
		OnPlayerJoinCenterDSDelegate.Broadcast(Join, PlayerInfo);
	}, 1.0f, false);
	
}



void AHallPlayerController::RPC_ClientPlayerIntoRoomDS_Implementation(int32 RoomDSPort, int32 MapID, bool IsXunLianMode, int32 RoomID)
{
	
	FString LType = TEXT("");
	
	if (GetInfoSubsystem()->ProcessType == EProcessType::PC_XYD)
	{
		LType = TEXT("PC_XYD");
	}
	else if (GetInfoSubsystem()->ProcessType == EProcessType::VR_XYD)
	{
		LType = TEXT("VR_XYD");
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ProcessType类型错误"));
	}
	
	//是训练模式还是考核模式
	FString LMode = IsXunLianMode ? TEXT("XunLianMode") : TEXT("KaoHeMode");
	
	// 拼接地址：IP:端口
	FString ConnectAddress = FString::Printf(TEXT("%s:%d?SN=%s?PlayerName=%s?ClientType=%s?Mode=%s?"), *GetInfoSubsystem()->DSIP, RoomDSPort, 
		*GetInfoSubsystem()->PlayerInfo.SN, *GetInfoSubsystem()->PlayerInfo.PlayerName, *LType, *LMode);
	
	GetInfoSubsystem()->PlayerInfo.MapID = MapID;
	GetInfoSubsystem()->PlayerInfo.RoomID = RoomID;
	
	//挂载课件pak
	UBFL_Common::MountPakFile(this,GetInfoSubsystem()->PlayerInfo.MapID);
	FString MapName = FString::Printf(TEXT("/KJ%d/MainMap"), MapID);
	
	
	FTimerHandle ConnectTimer;
	GetWorld()->GetTimerManager().SetTimer(ConnectTimer, [this, ConnectAddress, MapName]()
	{
		
		//UE_LOG(LogTemp, Log, TEXT("先本地打开地图：%s"), *MapName);
		UGameplayStatics::OpenLevel(this, *MapName);
		UE_LOG(LogTemp, Log, TEXT("学员端加载课件Pak文件后 延时下再去连接DS:%s"), *ConnectAddress);
		// bAbsolute=true 联网必须开绝对跳转
		UGameplayStatics::OpenLevel(this, FName(*ConnectAddress), true);
		//GetWorld()->GetFirstPlayerController()->ConsoleCommand(FString::Printf(TEXT("open %s"), *ConnectAddress));
	}, 1.0f, false);
	
	
}

void AHallPlayerController::RPC_ServerJLDNotifyPlayerIntoRoomDS_Implementation(int32 Port,
                                                                               int32 MapID, int32 RoomID, bool IsXunLianMode, const TArray<FString>& LocalSn)
{
	
	GHallGameMode->NotifyPlayerIntoRoomDS(Port, MapID, RoomID, IsXunLianMode, LocalSn);
	
}


void AHallPlayerController::PRC_ClientPlayerSetPlayerInfo_Implementation(FPlayerInfo Info)
{
	GetInfoSubsystem()->PlayerInfo = Info;
	GetInfoSubsystem()->PlayerStatus.SN = Info.SN;
	GetInfoSubsystem()->PlayerStatus.PlayerName = Info.PlayerName;
	UE_LOG(LogTemp, Log, TEXT("本地保存学员%s-编号%s-的相关信息"), *GetInfoSubsystem()->PlayerStatus.PlayerName, *GetInfoSubsystem()->PlayerStatus.SN);
	
}


void AHallPlayerController::RPC_ServerJLDStartRoomDS_Implementation(int32 MapID, bool LIsXunLianMode, const TArray<FString>& SnArray)
{
	//服务器上动态拉起房间DS
	FString Temp_Bat;
	
	GetInfoSubsystem()->CurrentRoomID++;
	GetInfoSubsystem()->TempRoomDSPort = GetInfoSubsystem()->GetNewRoomDSPort();
	
	// 将玩家SN数组保存到大厅GameMode的字典中
	if (GHallGameMode)
	{
		GHallGameMode->RoomWaitingPlayers.Add(GetInfoSubsystem()->CurrentRoomID, SnArray);
		GHallGameMode->RoomWaitingPlayersMode.Add(GetInfoSubsystem()->CurrentRoomID, LIsXunLianMode);
		UE_LOG(LogTemp, Log, TEXT("已将%d个玩家加入RoomID=%d的等待队列"), SnArray.Num(), GetInfoSubsystem()->CurrentRoomID);
	}
	
	UBFL_Common::GenerateBatchScript_RoomDS(GetInfoSubsystem()->TempRoomDSPort, MapID, GetInfoSubsystem()->CurrentRoomID, Temp_Bat);
	
	if (!Temp_Bat.IsEmpty())
	{
		UBFL_Common::ExecuteBatchScript(Temp_Bat);
		
	}else
	{
		UE_LOG(LogTemp, Error, TEXT("bat脚本路径为空了"));
		
	}
	
}


