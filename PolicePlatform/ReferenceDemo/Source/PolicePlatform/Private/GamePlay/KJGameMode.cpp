#include "GamePlay/KJGameMode.h"
#include "GameFramework/PlayerState.h"
#include "GamePlay/KJPlayerController.h"
#include "GamePlay/KJGameState.h"
#include "GamePlay/PPPlayerBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Network/WSSubsystem.h"
#include "Component/AC_XSL.h"
#include "TimerManager.h"// 提供延时设置默认监控学员功能


APlayerController* AKJGameMode::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal,
                                      const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	
	APlayerController* PC = Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);
	AKJPlayerController* KJPC = Cast<AKJPlayerController>(PC);
	
	//编辑器
	if (GIsEditor)
	{
		KJPC->EditorInit();
		KJPC->bIsXunLianMode = KJPC->bTestIsXunLianMode;
	}
	else
	{
		//非编辑器单机训练
		if (GetWorld()->GetNetMode() == NM_Standalone)
		{
			KJPC->StandaloneInit();
			
			const FString LMode = UGameplayStatics::ParseOption(Options, TEXT("Mode"));
			KJPC->bIsXunLianMode = (LMode == TEXT("XunLianMode")) ? true : false;
		}
		//非编辑器联网训练
		else
		{
			UE_LOG(LogTemp, Log, TEXT("玩家连接 - 地址: %s, Options: %s"), *Portal, *Options);
			const FString ClientType = UGameplayStatics::ParseOption(Options, TEXT("ClientType"));
			const FString LMode = UGameplayStatics::ParseOption(Options, TEXT("Mode"));
			
			DSClientTypeInit(Options, ClientType, KJPC);
			//是考核模式还是训练模式
			KJPC->bIsXunLianMode = (LMode == TEXT("XunLianMode")) ? true : false;
		}
	}
	
	
	return PC;
}

void AKJGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	AKJPlayerController* KJPC = Cast<AKJPlayerController>(NewPlayer);
	if (KJPC)
	{
		KJPC->ServerStart_BIE(KJPC->ProcessType, KJPC->GPlayerInfo);
		KJPC->RPC_ClientStart(KJPC->ProcessType, KJPC->GPlayerInfo);
	}
	
	//非编辑器运行且非单机模式
	if (!GIsEditor && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		if (KJPC->ProcessType==EProcessType::PC_XYD || KJPC->ProcessType==EProcessType::VR_XYD)
		{
			check(GWSSubsystem);
			//通知中心DS有玩家进入某个房间DS
			GWSSubsystem->WS_SendRoomDSPlayersChangeToCenterDS(KJPC->GPlayerInfo, true);

			//TODO，那如果待重新开始的玩家意外闪退无法加入DS这种情况就由问题了？？？
			if (RestartPlayerSN == KJPC->GPlayerInfo.SN)
			{
				RestartPlayerSN = TEXT("");
			}
			
			GSN_PCMap.Add(KJPC->GPlayerInfo.SN, KJPC);

			// 每名学员进入后都延时刷新一次像素流当前监控对象
// 如果已经存在监控学员，TrySetDefaultMonitoredStudent会重新通知像素流端
// 如果还没有监控学员，则会自动选择第一名有效学员
			FTimerHandle RefreshMonitorTimerHandle;
			GetWorldTimerManager().SetTimer(RefreshMonitorTimerHandle, this, &AKJGameMode::TrySetDefaultMonitoredStudent, 1.0f, false);
		}
	}
	
}

void AKJGameMode::Logout(AController* ExitingController)
{
	Super::Logout(ExitingController);
	
	AKJPlayerController* KJPC = Cast<AKJPlayerController>(ExitingController);
	
	//非编辑器运行且 DS模式
	if (!GIsEditor && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		if (KJPC->ProcessType==EProcessType::PC_XYD || KJPC->ProcessType==EProcessType::VR_XYD)
		{
			//通知中心DS有玩家离开某个房间DS
			GWSSubsystem->WS_SendRoomDSPlayersChangeToCenterDS(KJPC->GPlayerInfo, false);
			//清理学员控制器字典
			GSN_PCMap.Remove(KJPC->GPlayerInfo.SN);
			
			//有玩家要重新开始时不能关闭房间DS
			if (RestartPlayerSN.IsEmpty())
			{
				//检查是否需要关闭DS服务器
				//TODO 因为学员在结束时可以选择重新体验，则这里要处理下，不能立马关闭，有没有更好的处理方法？
				FTimerHandle TimerHandle;
				GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
				{
					CheckNeedCloseRoomDS();
				}, 0.5f, false);
			}
			
		}
	}
}

void AKJGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	
	FString LMapIDStr;
	FString LRoomIDStr;
	FString LPortStr;

	if (FParse::Value(FCommandLine::Get(), TEXT("-MapID="), LMapIDStr))
	{
		GRoomDSInfo.MapID = FCString::Atoi(*LMapIDStr);
		UE_LOG(LogTemp, Log, TEXT("解析启动参数 MapID: %d"), GRoomDSInfo.MapID);
	}

	if (FParse::Value(FCommandLine::Get(), TEXT("-RoomID="), LRoomIDStr))
	{
		GRoomDSInfo.RoomID = FCString::Atoi(*LRoomIDStr);
		UE_LOG(LogTemp, Log, TEXT("解析启动参数 RoomID: %d"), GRoomDSInfo.RoomID);
	}
	
	if (FParse::Value(FCommandLine::Get(), TEXT("-Port="), LPortStr))
	{
		GRoomDSInfo.Port = FCString::Atoi(*LPortStr);
		UE_LOG(LogTemp, Log, TEXT("解析启动参数 Port: %d"), GRoomDSInfo.Port);
	}
	
}


void AKJGameMode::BeginPlay()
{
	Super::BeginPlay();

	//绑定WS回调，把学员从当前房间DS中踢出
	/*暂时弃用了
	GWSSubsystem->OnKillPlayerFromRoomDS.AddDynamic(this, &AKJGameMode::KickPlayer);
	*/
}

void AKJGameMode::KickPlayer(const FString& LSN)
{
	if (GSN_PCMap.Contains(LSN))
	{
		APPPlayerControllerBase** LPC = GSN_PCMap.Find(LSN);
		if (LPC && *LPC)
		{
			AKJPlayerController* KJPC = Cast<AKJPlayerController>(*LPC);
			if (KJPC)
			{
				KJPC->RPC_ClientGoToHall();
				UE_LOG(LogTemp, Log, TEXT("把学员序号 %s 踢出房间DS"), *LSN);
			}
		}
	}
	
}


void AKJGameMode::SetPCGPlayerInfo(const FString& Options, AKJPlayerController* LKJPC)
{
	LKJPC->GPlayerInfo.SN = UGameplayStatics::ParseOption(Options, TEXT("SN"));
	LKJPC->GPlayerInfo.PlayerName = UGameplayStatics::ParseOption(Options, TEXT("PlayerName"));
	LKJPC->GPlayerInfo.MapID = GRoomDSInfo.MapID;
	LKJPC->GPlayerInfo.RoomID = GRoomDSInfo.RoomID;
		
	UE_LOG(LogTemp, Log, TEXT("解析玩家信息 - SN: %s, PlayerName: %s, MapID: %d, RoomID: %d"), 
		   *LKJPC->GPlayerInfo.SN, *LKJPC->GPlayerInfo.PlayerName, LKJPC->GPlayerInfo.MapID, LKJPC->GPlayerInfo.RoomID);
}

void AKJGameMode::SetXSLPCGPlayerInfo(AKJPlayerController* LKJPC)
{
	LKJPC->GPlayerInfo.MapID = GRoomDSInfo.MapID;
	LKJPC->GPlayerInfo.RoomID = GRoomDSInfo.RoomID;
}

void AKJGameMode::CheckNeedCloseRoomDS() const
{
	if (GSN_PCMap.IsEmpty())
	{
		if (XSL_PlayerController)
		{
			XSL_PlayerController->RPC_ClientExit(false);
		}
		GWSSubsystem->ExitRoomDS();
	}
}

void AKJGameMode::DSClientTypeInit(const FString& LOptions, const FString& LClientType, AKJPlayerController* LKJPC)
{
	if (!LClientType.IsEmpty())
	{
		if (LClientType == TEXT("ViewPort"))
		{
			UE_LOG(LogTemp, Log, TEXT("获取ClientType为ViewPort，连接进来的客户端是像素流客户端！"));
			LKJPC->ProcessType = EProcessType::ViewPort;
			
			LKJPC->PlayerState->SetIsOnlyASpectator(true);
			XSL_PlayerController = LKJPC;
			UE_LOG(LogTemp, Log, TEXT("已将ViewPort客户端设置为观察者模式"));

			// 创建像素流客户端连接后的监控状态恢复计时器
			FTimerHandle RestoreMonitorTimerHandle;

			// 等待学员Pawn复制完成后恢复当前监控对象
			GetWorldTimerManager().SetTimer(RestoreMonitorTimerHandle, this, &AKJGameMode::TrySetDefaultMonitoredStudent, 1.0f, false);
			
			SetXSLPCGPlayerInfo(LKJPC);
				
			if (LKJPC->XSLComponentClass)
			{
				LKJPC->XSLComponent = NewObject<UAC_XSL>(LKJPC, LKJPC->XSLComponentClass);
				LKJPC->XSLComponent->RegisterComponent();
				LKJPC->XSLComponent->Activate();
				UE_LOG(LogTemp, Log, TEXT("已为像素流控制器动态挂载AC_XSL组件"));
			}
			
		}else if (LClientType == TEXT("PC_XYD"))
		{
			UE_LOG(LogTemp, Log, TEXT("获取ClientType是PC_XYD，连接进来的是PC学员端！"));
			LKJPC->ProcessType = EProcessType::PC_XYD;
			
			SetPCGPlayerInfo(LOptions, LKJPC);
			
		}else if (LClientType == TEXT("VR_XYD"))
		{
			UE_LOG(LogTemp, Log, TEXT("获取ClientType是VR_XYD，连接进来的是VR学员端！"));
			LKJPC->ProcessType = EProcessType::VR_XYD;
			
			SetPCGPlayerInfo(LOptions, LKJPC);
		}
	}else
	{
		UE_LOG(LogTemp, Warning, TEXT("获取ClientType为空"));
	}
}

void AKJGameMode::JLDLookWhoChange(const FString& LSN)
{
	UE_LOG(LogActor, Log, TEXT("教练端当前查看视口玩家变化 %s"), *LSN);
	
	//通过SN在学员字典里查找对应的玩家控制器，再获取其APPPlayerBase
	APPPlayerBase* LookWho = nullptr;
	if (APPPlayerControllerBase** FoundPC = GSN_PCMap.Find(LSN))
	{
		if (FoundPC && *FoundPC)
		{
			LookWho = Cast<APPPlayerBase>((*FoundPC)->GetPawn());
		}
	}

	//更新教练端当前观看的玩家
	AKJGameState* KJGameState = Cast<AKJGameState>(GameState);
	if (KJGameState)
	{
		KJGameState->JLDLookWho = LookWho;
	}

	//通知像素流客户端教练端当前观看的玩家
	if (XSL_PlayerController)
	{
		XSL_PlayerController->RPC_ClientNotifyJLDLookWho(LookWho);
	}
}

// 自动设置或者恢复像素流默认监控学员
void AKJGameMode::TrySetDefaultMonitoredStudent()
{
	// 获取当前课件GameState
	AKJGameState* KJGameState = Cast<AKJGameState>(GameState);

	// GameState无效时不能继续处理
	if (!IsValid(KJGameState))
	{
		return;
	}

	// 已经存在监控学员时只需要重新通知像素流客户端
	if (IsValid(KJGameState->JLDLookWho))
	{
		if (IsValid(XSL_PlayerController))
		{
			XSL_PlayerController->RPC_ClientNotifyJLDLookWho(KJGameState->JLDLookWho);
		}

		return;
	}

	// 遍历当前房间中的全部学员
	for (const TPair<FString, APPPlayerControllerBase*>& PlayerPair : GSN_PCMap)
	{
		// 跳过无效学员控制器
		if (!IsValid(PlayerPair.Value))
		{
			continue;
		}

		// 获取学员Pawn
		APPPlayerBase* StudentPawn = Cast<APPPlayerBase>(PlayerPair.Value->GetPawn());

		// 跳过尚未生成Pawn的学员
		if (!IsValid(StudentPawn))
		{
			continue;
		}

		// 将找到的第一名有效学员设置为默认监控对象
		JLDLookWhoChange(PlayerPair.Key);

		// 输出自动选择日志
		UE_LOG(LogTemp, Warning, TEXT("自动设置像素流默认监控学员：学员=%s，SN=%s"), *GetNameSafe(StudentPawn), *PlayerPair.Key);

		// 已经找到默认学员，不再继续遍历
		return;
	}
}

void AKJGameMode::KillAllPlayer()
{
	//中断所有训练只对受控训练的房间有效
	if (GRoomDSInfo.bControlledMode)
	{
		for (auto& Item : GSN_PCMap)
		{
			KickPlayer(Item.Key);
		}
	}
	
}

void AKJGameMode::CalibrateTrainTime(const FString& LSn, int32 LRoomID, int32 LTrainTime)
{
	if (GSN_PCMap.Contains(LSn))
	{
		APPPlayerControllerBase** LPC = GSN_PCMap.Find(LSn);
		if (*LPC)
		{
			APPPlayerBase* LPlayer = Cast<APPPlayerBase>((*LPC)->GetPawn());
			if (LPlayer && LPlayer->AC_Task)
			{
				LPlayer->AC_Task->TrainTime = LTrainTime;
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("错误了 训练时间校准时 未找到对应的玩家"));
		}
	}
}
