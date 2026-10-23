// Fill out your copyright notice in the Description page of Project Settings.



#include "GamePlay/StartPlayerController.h"

#include "BFL/BFL_Common.h"
#include "Kismet/GameplayStatics.h"
#include "Tools/InfoSubsystem.h"

void AStartPlayerController::OnPossess(APawn* InPawn)
{
	
	//此时gameinstance里的初始化还未完成，所以这里要另外处理
	StartInit();
	
	Super::OnPossess(InPawn);
	
	
}

void AStartPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
}

void AStartPlayerController::StartInit()
{
	//获取并打印启动参数
	const FString CommandLine = FCommandLine::Get();
	
	//解析 -ClientType
	FString ClientType;
	FParse::Value(*CommandLine, TEXT("-ClientType="), ClientType);
	ClientType.TrimStartAndEndInline();
	
	if (ClientType.Equals(TEXT("ViewPort"), ESearchCase::IgnoreCase))
	{
		ProcessType = EProcessType::ViewPort;
	}
	else
	{
			
#if PLATFORM_WINDOWS
		
		//PC学员或者教练端
		ProcessType = EProcessType::None;
			
#elif PLATFORM_ANDROID
		
		ProcessType = EProcessType::VR_XYD;
			
#endif
		
	}
	
	UE_LOG(LogTemp, Log, TEXT("单机开始地图时枚举类型是AStartPlayerController- ProcessType = %s"), *UEnum::GetValueAsString(ProcessType));
	
	//UInfoSubsystem里的ProcessType值为ViewPort代表的是房间DS异常关闭，像素流客户端返回到里开始地图，这种情况需要退出进程
	if (ProcessType==EProcessType::ViewPort && GetGameInstance()->GetSubsystem<UInfoSubsystem>()->ProcessType == EProcessType::ViewPort)
	{
		UE_LOG(LogTemp, Error, TEXT("错误，房间DS异常关闭，像素流客户端和房间DS断开连接，手动退出进程"));
		FGenericPlatformMisc::RequestExit(false);
	}
	
	ServerStart_BIE(ProcessType, FPlayerInfo());
	
}

void AStartPlayerController::StartStandaloneKJMap(int32 LMapID, bool LIsXunLianMode)
{
	GetGameInstance()->GetSubsystem<UInfoSubsystem>()->PlayerInfo.MapID = LMapID;
	
	/*
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
	
	
	// 拼接地址
	FString ConnectAddress = FString::Printf(TEXT("/KJ%d/MainMap?SN=%s?PlayerName=%s?ClientType=%s?Mode=XunLianMode?"), MapID, 
		*GetInfoSubsystem()->PlayerInfo.SN, *GetInfoSubsystem()->PlayerInfo.PlayerName, *LType);
	*/
	
	//挂载课件pak
	UBFL_Common::MountPakFile(this,LMapID);
	//是训练模式还是考核模式
	FString LMode = LIsXunLianMode ? TEXT("XunLianMode") : TEXT("KaoHeMode");
	FString ConnectAddress = FString::Printf(TEXT("/KJ%d/MainMap?Mode=%s?"), LMapID, *LMode);
	
	
	FTimerHandle ConnectTimer;
	GetWorld()->GetTimerManager().SetTimer(ConnectTimer, [this, ConnectAddress]()
	{
		UE_LOG(LogTemp, Log, TEXT("单机模式打开课件地图：%s"), *ConnectAddress);
		UGameplayStatics::OpenLevel(this, FName(*ConnectAddress), true);
	}, 0.2f, false);
	
	
}
