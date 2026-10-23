// Fill out your copyright notice in the Description page of Project Settings.

#include "GamePlay/PPGameInstance.h"
#include "BFL/BFL_Common.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Network/WSSubsystem.h"
#include "Tools/InfoSubsystem.h"

void UPPGameInstance::Init()
{
	Super::Init();
	
	
}

void UPPGameInstance::Shutdown()
{
	Super::Shutdown();


}

void UPPGameInstance::OnStart()
{
	GInfoSubsystem = this->GetSubsystem<UInfoSubsystem>();
	check(GInfoSubsystem);
	GInfoSubsystem->GetConfig();
	
	StartInit();
	
	Super::OnStart();
}


void UPPGameInstance::RoomDSInit(const FString& CommandLine)
{
	GInfoSubsystem->SetProcessType(EProcessType::RoomDS);
	
	FString Port, MapID, RoomID;
	UWSSubsystem* WSSubsystem = this->GetSubsystem<UWSSubsystem>();
	check(WSSubsystem);
		
	FParse::Value(*CommandLine, TEXT("-RoomID="), RoomID);
	FParse::Value(*CommandLine, TEXT("-MapID="), MapID);
	FParse::Value(*CommandLine, TEXT("-Port="), Port);
	RoomID.TrimStartAndEndInline();
	MapID.TrimStartAndEndInline();
	Port.TrimStartAndEndInline();
			
	GInfoSubsystem->RoomDSInfo.MapID = FCString::Atoi(*MapID);
	GInfoSubsystem->RoomDSInfo.RoomID = FCString::Atoi(*RoomID);
	GInfoSubsystem->RoomDSInfo.Port = FCString::Atoi(*Port);
			
	UBFL_Common::MountPakFile(this,GInfoSubsystem->RoomDSInfo.MapID);
			
	FString ConnectAddress = FString::Printf(TEXT("/KJ%d/MainMap"), GInfoSubsystem->RoomDSInfo.MapID);
	
	UE_LOG(LogTemp, Log, TEXT("ConnectAddress的值是:%s"), *ConnectAddress);
	
	//TODO 延时下打开，后续测试不需要就删除
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this, ConnectAddress, WSSubsystem]()
	{
		UE_LOG(LogTemp, Log, TEXT("房间DS打开地图:%s"), *ConnectAddress);
		// 连接到房间DS
		UGameplayStatics::OpenLevel(this, FName(*ConnectAddress));
		//ws连接中心DS,要确保DS打开了对应的课件地图才能通知玩家端加入到DS
		WSSubsystem->StartWS_RoomDS();
	}, 1.0f, false);
}

void UPPGameInstance::ViewPortInit(const FString& CommandLine)
{
	//像素流客户端设置为固定帧率
	GEngine->FixedFrameRate = 40;
	GEngine->bUseFixedFrameRate = true;
	
	GInfoSubsystem->SetProcessType(EProcessType::ViewPort);
	FString Port, MapID, RoomID;
	
	//像素流客户端 - 直接连接房间服务器
	FParse::Value(*CommandLine, TEXT("-Port="), Port);
	FParse::Value(*CommandLine, TEXT("-MapID="), MapID);
	FParse::Value(*CommandLine, TEXT("-RoomID="), RoomID);
	Port.TrimStartAndEndInline();
	MapID.TrimStartAndEndInline();
	RoomID.TrimStartAndEndInline();

	GInfoSubsystem->PlayerInfo.MapID = FCString::Atoi(*MapID);
	GInfoSubsystem->PlayerInfo.RoomID = FCString::Atoi(*RoomID);
	
	const FString URL = FString::Printf(
		TEXT("%s:%s?ClientType=ViewPort?MapID=%s?RoomID=%s"),
		*GInfoSubsystem->DSIP, *Port, *MapID, *RoomID);

	
		 
	//挂载对应的课件pak
	UBFL_Common::MountPakFile(this, FCString::Atoi(*MapID));
	FString MapName = FString::Printf(TEXT("/KJ%s/MainMap"), *MapID);
	
	
	//TODO 延时下再去连接房间DS
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this, URL, MapName]()
	{
		if (!IsValid(this)) return;		
		//UE_LOG(LogTemp, Log, TEXT("先本地打开地图：%s"), *MapName);
		UGameplayStatics::OpenLevel(this, *MapName);
		UE_LOG(LogTemp, Log, TEXT("像素流客户端挂载课件pak后 延时下再去连接DS: %s"), *URL);
		UGameplayStatics::OpenLevel(this, FName(*URL), true);
	}, 1.0f, false);
			
	
}


void UPPGameInstance::StartInit()
{
	//获取并打印启动参数
	const FString CommandLine = FCommandLine::Get();
	UE_LOG(LogTemp, Log, TEXT("[启动初始化] 启动参数: %s"), *CommandLine);
	
	//服务器
	if (IsRunningDedicatedServer())
	{
		FString MapID;
		FParse::Value(*CommandLine, TEXT("-MapID="), MapID);
		MapID.TrimStartAndEndInline();
		
		
		if (MapID.Equals(TEXT("CenterDS"), ESearchCase::IgnoreCase))
		{
			UWSSubsystem* WSSubsystem = this->GetSubsystem<UWSSubsystem>();
			check(WSSubsystem);
			
			GInfoSubsystem->SetProcessType(EProcessType::CenterDS);
			WSSubsystem->StartWS_CenterDS();
		}
		else
		{
			RoomDSInit(CommandLine);
		}
	}
	//客户端
	else
	{
		//客户端 - 显示帧率
		if (GEngine)
		{
			GEngine->Exec(GetWorld(), TEXT("stat fps"));
		}
		

		//解析 -ClientType
		FString ClientType;
		FParse::Value(*CommandLine, TEXT("-ClientType="), ClientType);
		ClientType.TrimStartAndEndInline();

		UE_LOG(LogTemp, Log, TEXT("客户端类型ClientType是:%s"), *ClientType);
		if (ClientType.Equals(TEXT("ViewPort"), ESearchCase::IgnoreCase))
		{
			ViewPortInit(CommandLine);
		}
		else
		{
			//教练端和学员端都要去获取课件信息
			GInfoSubsystem->GetExistingKJ();
#if PLATFORM_WINDOWS
			
			UE_LOG(LogTemp, Log, TEXT("客户端平台：Windows"));
			// Windows：开启平滑帧率 
			GEngine->bSmoothFrameRate = true;
			GEngine->SmoothedFrameRateRange = FFloatRange(30.0f, 60.0f);
			// 关闭固定帧率（避免冲突）
			GEngine->bUseFixedFrameRate = false;
			//教练端和玩家端的入口要在AHallGameMode::PostLogin里服务器处理，按open连接服务器时带的ClientType区分
			UE_LOG(LogTemp, Log, TEXT("我是PC学员端或者教练端，待点击UI选择"));
			
			
#elif PLATFORM_ANDROID
			//vr头显不建议开启平滑帧率
			UE_LOG(LogTemp, Log, TEXT("客户端平台：Android"));
			GInfoSubsystem->SetProcessType(EProcessType::VR_XYD);
#endif
			
		}
	}
}




