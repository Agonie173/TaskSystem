#include "GamePlay/PPPlayerControllerBase.h"

#include "PXR_HMDFunctionLibrary.h"
#include "PXR_InputFunctionLibrary.h"
#include "GamePlay/PPPlayerBase.h"
#include "Component/AC_PCPlayer.h"
#include "Component/AC_VRPlayer.h"
#include "Net/UnrealNetwork.h"
#include "Network/WSSubsystem.h"
#include "Tools/InfoSubsystem.h"

APPPlayerControllerBase::APPPlayerControllerBase()
{
	
}

void APPPlayerControllerBase::BeginPlay()
{
	Super::BeginPlay();
}

void APPPlayerControllerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TimerHandle_VR_ReportHeadsetBattery.IsValid())
	{
		FTimerManager& TimerMgr = GetWorld()->GetTimerManager();
		TimerMgr.ClearTimer(TimerHandle_VR_ReportHeadsetBattery);
	}
	
	Super::EndPlay(EndPlayReason);
}

void APPPlayerControllerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(APPPlayerControllerBase, ProcessType);
}

void APPPlayerControllerBase::NotifyPlayerStatusChange(FPlayerStatus LPlayerStatus)
{
	UE_LOG(LogTemp, Log, TEXT("在AHallPlayerController子类里重写"));
}


UWSSubsystem* APPPlayerControllerBase::GetWSSubsystem()
{
	if (!GWSSubsystem)
	{
		GWSSubsystem = GetGameInstance()->GetSubsystem<UWSSubsystem>();
	}
	
	return GWSSubsystem;
}

UInfoSubsystem* APPPlayerControllerBase::GetInfoSubsystem()
{
	if (!GInfoSubsystem)
	{
		GInfoSubsystem = GetGameInstance()->GetSubsystem<UInfoSubsystem>();
	}
	
	return GInfoSubsystem;
}

void APPPlayerControllerBase::VR_ReportBattery(bool LIsHallPlayer)
{
	
	if (GIsEditor)
	{
		return;
	}
	
	int32 LHMDNum = -1;
	UPICOXRHMDFunctionLibrary::PXR_GetHmdBatteryLevel(LHMDNum);
	
	//拿的右手手柄电量
	int32 LTouchNum = -1;
	UPICOXRInputFunctionLibrary::PXR_GetControllerPower(EPICOXRControllerType::RightHand, LTouchNum);
	UE_LOG(LogTemp, Log, TEXT("获取到手柄电量: %d"), LTouchNum);
	
	//状态变化时才上报
	if ((LHMDNum != GetInfoSubsystem()->PlayerStatus.HeadsetBattery) || (LTouchNum != GetInfoSubsystem()->PlayerStatus.TouchBattery) )
	{
		GetInfoSubsystem()->PlayerStatus.HeadsetBattery = LHMDNum;
		GetInfoSubsystem()->PlayerStatus.TouchBattery = LTouchNum;
		RPC_ServerNotifyPlayerStatusChange(LIsHallPlayer, GetInfoSubsystem()->PlayerStatus);
		UE_LOG(LogTemp, Log, TEXT("上报了玩家状态"));
	}
	
}

void APPPlayerControllerBase::EditorInit()
{
	//编辑器里测试
	if (GIsEditor)
	{
		if (GetWorld()->GetNetMode() < NM_ListenServer)
		{
			if (bTestVR)
			{
				ProcessType = EProcessType::VR_XYD;
			}
			else
			{
				ProcessType = EProcessType::PC_XYD;
			}
			
			UE_LOG(LogTemp, Log, TEXT("类型是 %s"), *UEnum::GetValueAsString(ProcessType));
			
		}
	}
	
}

void APPPlayerControllerBase::StandaloneInit()
{
	//单机情况：一是启动地图，二是单机模式跑课件内容
	if (GetWorld()->GetNetMode() == NM_Standalone)
	{
		ProcessType = GetInfoSubsystem()->ProcessType;
		
		/*
		//单机训练模式
#if PLATFORM_WINDOWS

		//像素流客户端启动时不要创建PCStartUI,像素流启动时在gameinstance里会先赋值为EProcessType::ViewPort
		if (GetInfoSubsystem()->ProcessType == EProcessType::ViewPort)
		{
			ProcessType = EProcessType::ViewPort;
		}
		else
		{
			ProcessType = EProcessType::PC_XYD;
		}

		
			
#elif PLATFORM_ANDROID
			
		UE_LOG(LogTemp, Log, TEXT("我是单机模式VR学员端！"));
		ProcessType = EProcessType::VR_XYD;
			
#endif
		*/
	}
}

void APPPlayerControllerBase::RPC_ServerNotifyPlayerStatusChange_Implementation(bool IsHallPlayer, FPlayerStatus LPlayerStatus)
{
	if(IsHallPlayer)
	{
		UE_LOG(LogTemp, Log, TEXT("收到大厅DS玩家状态变化"));
		NotifyPlayerStatusChange(LPlayerStatus);
	}
	else
	{
		//房间DS学员走ws
		if (GetWSSubsystem())
		{
			UE_LOG(LogTemp, Log, TEXT("收到房间DS玩家状态变化"));
			GetWSSubsystem()->WS_SendPlayerStatusChangeToCenterDS(LPlayerStatus);
		}
		
	}
}

void APPPlayerControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	if(HasAuthority())
	{
		APPPlayerBase* LPlayer = Cast<APPPlayerBase>(InPawn);
		check(LPlayer);
		
		LPlayer->ProcessType = ProcessType;
		LPlayer->GPlayerInfo = GPlayerInfo;

		//注意，客户端PC启动时，ProcessType值为EProcessType::none,此时不会去挂载组件，好像也不影响，因为此时显示的是UI
		if (LPlayer->ProcessType == EProcessType::PC_XYD)
		{
			if (LPlayer->AC_PCClass && !LPlayer->AC_PCPlayer)
			{
				LPlayer->AC_PCPlayer = NewObject<UAC_PCPlayer>(LPlayer, LPlayer->AC_PCClass);
				LPlayer->AC_PCPlayer->SetIsReplicated(true);
				LPlayer->AC_PCPlayer->RegisterComponent();
				LPlayer->AC_PCPlayer->Activate();
				UE_LOG(LogTemp, Log, TEXT("已为PC学员端动态挂载AC_PCPlayer组件"));
			}
		}else if (LPlayer->ProcessType == EProcessType::VR_XYD)
		{
			if (LPlayer->AC_VRClass && !LPlayer->AC_VRPlayer)
			{
				LPlayer->AC_VRPlayer = NewObject<UAC_VRPlayer>(LPlayer, LPlayer->AC_VRClass);
				LPlayer->AC_VRPlayer->SetIsReplicated(true);
				LPlayer->AC_VRPlayer->RegisterComponent();
				LPlayer->AC_VRPlayer->Activate();
				UE_LOG(LogTemp, Log, TEXT("已为VR学员端动态挂载AC_VRPlayer组件"));
			}
		}
		else
		{
			
		}
		
		//服务器的onRep得手动调用
		LPlayer->OnRep_ProcessType();
	}
}
