// Fill out your copyright notice in the Description page of Project Settings.


#include "GamePlay/HallGameMode.h"
#include "GamePlay/HallPlayerController.h"
#include "GamePlay/HallGameState.h"
#include "GameFramework/PlayerState.h"
#include "Tools/InfoSubsystem.h"
#include "GamePlay/PPGameInstance.h"
#include "Kismet/GameplayStatics.h"

AHallGameMode::AHallGameMode()
{
}

void AHallGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	HallGameState = Cast<AHallGameState>(GetWorld()->GetGameState());
	PPGameInstance = Cast<UPPGameInstance>(GetGameInstance());
	InfoSubsystem = GetGameInstance()->GetSubsystem<UInfoSubsystem>();
	
}

APlayerController* AHallGameMode::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal,
                                        const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	APlayerController* PC = Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);
	
	UE_LOG(LogTemp, Log, TEXT("玩家连接 - 地址: %s, Options: %s"), *Portal, *Options);
	AHallPlayerController* HallPC = Cast<AHallPlayerController>(PC);
	const FString ClientType = UGameplayStatics::ParseOption(Options, TEXT("ClientType"));
	
	if (!ClientType.IsEmpty())
	{
		if (ClientType == TEXT("JLD"))
		{
			if (JLD_PC)
			{
				UE_LOG(LogTemp, Error, TEXT("只能只有一个教练端"));
				Logout(PC);
			}
			
			UE_LOG(LogTemp, Log, TEXT("连接进来的是教练端"));
			HallPC->ProcessType = EProcessType::JLD;
			HallPC->PlayerState->SetIsOnlyASpectator(true);
			UE_LOG(LogTemp, Log, TEXT("已将教练端设置为观察者模式"));
			
		}else if (ClientType == TEXT("PC_XYD"))
		{
			UE_LOG(LogTemp, Log, TEXT("连接进来的是PC学员端"));
			HallPC->ProcessType = EProcessType::PC_XYD;
			
		}else if (ClientType == TEXT("VR_XYD"))
		{
			UE_LOG(LogTemp, Log, TEXT("连接进来的是VR学员端"));
			HallPC->ProcessType = EProcessType::VR_XYD;
			
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("错误获取ClientType失败！"));
	}
	
	
	//有值代表是从房间DS返回中心DS的玩家,所以从房间DS返回后得带启动参数连接中心DS
	HallPC->GPlayerInfo.SN = UGameplayStatics::ParseOption(Options, TEXT("SN"));
	HallPC->GPlayerInfo.PlayerName = UGameplayStatics::ParseOption(Options, TEXT("PlayerName"));
	
	return PC;
}

void AHallGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	AHallPlayerController* PC = Cast<AHallPlayerController>(NewPlayer);
	check(PC);
	
	//PostLogin里调用RPC才是安全的
	PC->ServerStart_BIE(PC->ProcessType, PC->GPlayerInfo);
	PC->RPC_ClientStart(PC->ProcessType, PC->GPlayerInfo);
	
	//根据客户端的类型触发不同的逻辑
	if (PC->ProcessType == EProcessType::JLD)
	{
		JLD_PC = PC;
		TArray<AActor*> LAllActor;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(),AHallPlayerController::StaticClass(),LAllActor);
		//把先教练端加入中心DS的学员在教练端显示出来
		for (AActor* Item : LAllActor)
		{
			if ( IsValid(Item))
			{
				AHallPlayerController* LPC = Cast<AHallPlayerController>(Item);
				if (LPC->ProcessType == EProcessType::PC_XYD || LPC->ProcessType == EProcessType::VR_XYD)
				{
					PlayerIntoHallDS(LPC);
				}
			}
		}
		
	}else if (PC->ProcessType == EProcessType::PC_XYD || PC->ProcessType == EProcessType::VR_XYD)
	{
		if (JLD_PC)
		{
			PlayerIntoHallDS(PC);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("客户端类型错误,注意这句报错只会在中心DS出现"));
	}
	
}

void AHallGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	
	AHallPlayerController* PC = Cast<AHallPlayerController>(Exiting);
	if (PC && PC->ProcessType == EProcessType::JLD)
	{
		// 教练端断开连接，关闭服务器
		//UE_LOG(LogTemp, Error, TEXT("教练端断开连接，服务器即将关闭"));
		
		//FGenericPlatformMisc::RequestExit(false);
	}else if (PC && PC->ProcessType == EProcessType::PC_XYD || PC->ProcessType == EProcessType::VR_XYD)
	{
		FPlayerInfo LPlayerInfo;
		LPlayerInfo.SN = PC->GPlayerInfo.SN;
		UE_LOG(LogTemp, Log, TEXT("学员端%s 离开大厅"), *PC->GPlayerInfo.PlayerName);
		HallGameState->PlayerInfoListRemoveItem(LPlayerInfo.SN, true);
		
		if (JLD_PC)
		{
			JLD_PC->RPC_ClientJLDPlayerJoinCenterDS(false, LPlayerInfo);
		}
		
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("错误：客户端退出大厅时类型有问题"));
	}
	
}


void AHallGameMode::PlayerIntoHallDS(AHallPlayerController* PC)
{
	check(JLD_PC);
	
	// 创建玩家信息结构体
	FPlayerInfo LPlayerInfo;
	//玩家第一次连接进来
	if (PC->GPlayerInfo.SN.IsEmpty())
	{
		InfoSubsystem->TempNum++;
			
		// 生成玩家名字（如"2号玩家"）
		FString PlayerName = FString::Printf(TEXT("%d号玩家"), InfoSubsystem->TempNum);
		
		LPlayerInfo.SN = FString::FromInt(InfoSubsystem->TempNum);
		UE_LOG(LogTemp, Log, TEXT("%s 编号"), *LPlayerInfo.SN);
		LPlayerInfo.PlayerName = PlayerName;
		LPlayerInfo.PC = PC;
		
		PC->GPlayerInfo = LPlayerInfo;
	
		
		PC->PRC_ClientPlayerSetPlayerInfo(LPlayerInfo);
		HallGameState->DaTingPlayers.Add(LPlayerInfo);
		JLD_PC->RPC_ClientJLDPlayerJoinCenterDS(true, LPlayerInfo);
		UE_LOG(LogTemp, Log, TEXT("%s 新玩家第一次进入大厅"), *LPlayerInfo.PlayerName);
		
		
	}
	//玩家从房间DS返回中心DS
	else
	{
		LPlayerInfo.SN = PC->GPlayerInfo.SN;
		LPlayerInfo.PlayerName = PC->GPlayerInfo.PlayerName;
		LPlayerInfo.PC = PC;
		
		HallGameState->DaTingPlayers.Add(LPlayerInfo);
		JLD_PC->RPC_ClientJLDPlayerJoinCenterDS(true, LPlayerInfo);
		UE_LOG(LogTemp, Log, TEXT("%s 玩家重新进入大厅"), *LPlayerInfo.PlayerName);
	}
	
}

void AHallGameMode::NotifyPlayerIntoRoomDS(int32 RoomDSPort, int32 MapID, int32 RoomID, bool IsXunLianMode, const TArray<FString>& Sn)
{
	// 遍历玩家名字数组
	for (const FString& LocalSN : Sn)
	{
		// 在PlayerInfoList中查找匹配的玩家
		for (const FPlayerInfo& Info : HallGameState->DaTingPlayers)
		{
			if (Info.SN == LocalSN)
			{
				if (Info.PC)
				{
					Info.PC->RPC_ClientPlayerIntoRoomDS(RoomDSPort, MapID, IsXunLianMode, RoomID);
					UE_LOG(LogTemp, Log, TEXT("通知编号%s玩家进入房间ID为%d的房间"), *LocalSN, RoomID);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("出错了，玩家对应的控制器为空了"));
				}
				break; // 找到后退出内层循环
			}
		}
	}
	
}