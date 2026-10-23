#include "GamePlay/KJPlayerController.h"
#include "GamePlay/PPPlayerBase.h"
#include "BFL/BFL_Common.h"
#include "GamePlay/KJGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Tools/InfoSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Network/WSSubsystem.h"


void AKJPlayerController::BeginPlay()
{
	if (HasAuthority())
	{
		// 缓存大厅游戏模式引用
		GKJGameMode = Cast<AKJGameMode>(GetWorld()->GetAuthGameMode());
	}
	
	GKJGameState = Cast<AKJGameState>(GetWorld()->GetGameState());
	
	Super::BeginPlay();
}

void AKJPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	//挂载任务组件
	if (HasAuthority())
	{
		APPPlayerBase* PlayerBase = Cast<APPPlayerBase>(InPawn);
		if (PlayerBase)
		{
			PlayerBase->bIsXunLianMode = bIsXunLianMode;
			PlayerBase->AttachAC_Task();
		}
	
	}
}

void AKJPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	
	if (GetWorld()->GetNetMode() == NM_Standalone || GetWorld()->GetNetMode() == NM_Client)
	{
		if (ProcessType == EProcessType::ViewPort)
		{
			
		}
		else
		{
			//重新体验退出时不需要卸载课件
			if (!bRestart)
			{
				UE_LOG(LogTemp,Log,TEXT("学员端卸载课件pak"));
				UBFL_Common::UnmountPakFile(this, GetInfoSubsystem()->PlayerInfo.MapID);
			}
				
		}
	}
	
	Super::EndPlay(EndPlayReason);
}

void AKJPlayerController::RPC_ClientStart_Implementation(EProcessType LProcessType, const FPlayerInfo& LPlayerInfo)
{
	
	if (LProcessType == EProcessType::VR_XYD)
	{
		//先上报一次
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_VR_ReportHeadsetBattery, [this]()
		{
			if (IsValid(this))
			{
				VR_ReportBattery(false);
			}
			
		}, 5.0f, true);
		
	}
	
	
	ClientStart_BIE(LProcessType, LPlayerInfo);
	
}


void AKJPlayerController::RPC_ServerKickPlayer_Implementation(const FString& LSn)
{
	GKJGameMode->KickPlayer(LSn);
}

void AKJPlayerController::RPC_ServerNotiyLookWho_Implementation(const FString& LSN)
{
	GetWSSubsystem()->WS_SendLookWhoToCenterDS(LSN);
}

void AKJPlayerController::RPC_ClientNotifyJLDLookWho_Implementation(APPPlayerBase* LLookWho)
{
	/*UE_LOG(LogTemp, Log, TEXT("教练端当前查看视口玩家变化OnJLDLookWhoChangeDelegate广播成功"));
	GKJGameState->OnJLDLookWhoChangeDelegate.Broadcast(LLookWho);*/

	// 获取当前进程的信息子系统
	UInfoSubsystem* InfoSubsystem = GetInfoSubsystem();

	// 判断当前是否为有效的像素流客户端
	if (IsValid(InfoSubsystem) && InfoSubsystem->ProcessType == EProcessType::ViewPort)
	{
		// 从服务器传入的学员对象中取得唯一SN，学员无效时使用空字符串
		const FString MonitoredStudentSN = IsValid(LLookWho) ? LLookWho->GPlayerInfo.SN : FString();

		// 在像素流客户端本地保存当前监控学员SN并广播变化事件
		InfoSubsystem->SetCurrentMonitoredStudentSN(MonitoredStudentSN);

		// 输出像素流客户端最终收到的监控学员
		UE_LOG(LogTemp, Warning, TEXT("像素流客户端收到监控学员变化：学员=%s，SN=%s"), *GetNameSafe(LLookWho), MonitoredStudentSN.IsEmpty() ? TEXT("无") : *MonitoredStudentSN);
	}
	else
	{
		// 理论上这个客户端RPC只会发送给像素流客户端
		UE_LOG(LogTemp, Warning, TEXT("监控学员变化未写入InfoSubsystem：当前进程不是ViewPort或InfoSubsystem无效"));
	}

	// 保留原有GameState委托，继续通知像素流蓝图切换主监控画面
	if (IsValid(GKJGameState))
	{
		GKJGameState->OnJLDLookWhoChangeDelegate.Broadcast(LLookWho);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("像素流监控画面切换失败：GKJGameState无效"));
	}
}

void AKJPlayerController::RPC_ServerPlayerRestart_Implementation(const FString& LSn)
{
	if (GKJGameMode->GSN_PCMap.Contains(LSn))
	{
		APPPlayerControllerBase** LPC = GKJGameMode->GSN_PCMap.Find(LSn);
		if (LPC && *LPC)
		{
			AKJPlayerController* KJPC = Cast<AKJPlayerController>(*LPC);
			if (KJPC)
			{
				UE_LOG(LogTemp, Log, TEXT("学员 %s 重新开始体验"), *LSn);
				GKJGameMode->RestartPlayerSN = LSn;
				KJPC->RPC_ClientRestartTrain(GKJGameMode->GRoomDSInfo.Port);
			}
		}
	}
}

void AKJPlayerController::RPC_ClientRestartTrain_Implementation(int32 LPort)
{
	UE_LOG(LogTemp, Log, TEXT("重新开始体验"));
	
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
	
	APPPlayerBase* LPlayer = Cast<APPPlayerBase>(GetPawn());
	
	
	//是训练模式还是考核模式
	FString LMode = LPlayer->bIsXunLianMode ? TEXT("XunLianMode") : TEXT("KaoHeMode");
	
	// 拼接地址：IP:端口
	FString ConnectAddress = FString::Printf(TEXT("%s:%d?SN=%s?PlayerName=%s?ClientType=%s?Mode=%s?"), *GetInfoSubsystem()->DSIP, LPort, 
		*GetInfoSubsystem()->PlayerInfo.SN, *GetInfoSubsystem()->PlayerInfo.PlayerName, *LType, *LMode);
	
	bRestart = true;
	UGameplayStatics::OpenLevel(this, FName(*ConnectAddress), true);
}


void AKJPlayerController::RPC_ServerPauseTrain_Implementation(bool LPause, const FString& LSn)
{
	// 游戏模式无效时无法根据SN查找目标学员
	if (!IsValid(GKJGameMode))
	{
		UE_LOG(LogTemp, Error, TEXT("暂停训练失败：GKJGameMode无效，学员SN=%s"), *LSn);
		return;
	}

	// 根据学员SN查找服务器上的目标PlayerController
	APPPlayerControllerBase** TargetControllerPtr = GKJGameMode->GSN_PCMap.Find(LSn);

	// 没有找到目标学员控制器时停止处理
	if (!TargetControllerPtr || !IsValid(*TargetControllerPtr))
	{
		UE_LOG(LogTemp, Error, TEXT("暂停训练失败：没有找到学员控制器，学员SN=%s"), *LSn);
		return;
	}

	// 获取目标学员控制的Pawn
	APPPlayerBase* TargetPlayer = Cast<APPPlayerBase>((*TargetControllerPtr)->GetPawn());

	// 目标Pawn或任务组件无效时不能设置暂停状态
	if (!IsValid(TargetPlayer) || !IsValid(TargetPlayer->AC_Task))
	{
		UE_LOG(LogTemp, Error, TEXT("暂停训练失败：目标学员或任务组件无效，学员SN=%s"), *LSn);
		return;
	}

	// 在服务器上修改该学员唯一的权威暂停状态
	TargetPlayer->AC_Task->SetTrainPaused(LPause);

	// 输出服务器处理结果
	UE_LOG(LogTemp, Warning, TEXT("服务器设置学员训练状态：学员SN=%s，暂停=%s"), *LSn, LPause ? TEXT("是") : TEXT("否"));

	// Dedicated Server把暂停或恢复结果通知中心服务器
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		UWSSubsystem* WSSubsystem = GetWSSubsystem();

		// WebSocket子系统有效时发送通知
		if (IsValid(WSSubsystem))
		{
			WSSubsystem->WS_SendPauseTrainToCenterDS(LSn, TargetPlayer->GPlayerInfo.RoomID, LPause);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("发送暂停训练结果失败：WSSubsystem无效，学员SN=%s"), *LSn);
		}
	}
}

void AKJPlayerController::GoToHall()
{
	UE_LOG(LogTemp, Log, TEXT("即将返回大厅"));
	
	FString LType = TEXT("");
	if (GetInfoSubsystem()->ProcessType == EProcessType::PC_XYD)
	{
		LType = TEXT("PC_XYD");
	}else if (GetInfoSubsystem()->ProcessType == EProcessType::VR_XYD)
	{
		LType = TEXT("VR_XYD");
	}
	FString ConnectAddress = FString::Printf(TEXT("%s:7777?ClientType=%s?SN=%s?PlayerName=%s"), 
			*GetInfoSubsystem()->DSIP, 
			*LType,
			*GetInfoSubsystem()->PlayerInfo.SN, 
			*GetInfoSubsystem()->PlayerInfo.PlayerName);
		
	UE_LOG(LogTemp, Log, TEXT("连接大厅服务器: %s"), *ConnectAddress);
	UGameplayStatics::OpenLevel(this, FName(*ConnectAddress), true);
}

void AKJPlayerController::RPC_ClientExit_Implementation(bool bForce)
{
	UE_LOG(LogTemp, Log, TEXT("客户端退出"));
	FGenericPlatformMisc::RequestExit(bForce);
}

void AKJPlayerController::RPC_ClientGoToHall_Implementation()
{
	GoToHall();
}

void AKJPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AKJPlayerController, XSLComponent);
}
