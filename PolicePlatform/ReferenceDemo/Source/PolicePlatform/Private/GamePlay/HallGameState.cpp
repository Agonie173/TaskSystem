// Fill out your copyright notice in the Description page of Project Settings.


#include "GamePlay/HallGameState.h"
#include "Net/UnrealNetwork.h"

void AHallGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHallGameState, DaTingPlayers);
	DOREPLIFETIME(AHallGameState, RoomDSPlayers);
	DOREPLIFETIME(AHallGameState, PlayerStatuses);
	DOREPLIFETIME(AHallGameState, AllRoomDS);
	
}

//注意这里的OnRep服务器上都不会触发，需要另外手动触发
void AHallGameState::OnRep_DaTingPlayerInfoList()
{
	UE_LOG(LogTemp, Log, TEXT("大厅玩家已更新，当前玩家数量：%d"), DaTingPlayers.Num());
	OnDaTingPlayerChange.Broadcast(DaTingPlayers);
}

void AHallGameState::OnRep_RoomDSPlayerInfoList()
{
	UE_LOG(LogTemp, Log, TEXT("房间DS玩家已更新，当前玩家数量：%d"), RoomDSPlayers.Num());
	OnRoomDSPlayerChange.Broadcast(RoomDSPlayers);
}

void AHallGameState::OnRep_PlayerStatuses()
{
	UE_LOG(LogTemp, Log, TEXT("有玩家的状态变化了"));
	OnPlayerStatusesChange.Broadcast(PlayerStatuses);
}

void AHallGameState::OnRep_RoomDSInfo()
{
	UE_LOG(LogTemp, Log, TEXT("房间DS有拉起关闭变化"));
	OnRoomDSInfoCahnge.Broadcast(AllRoomDS);
}

void AHallGameState::PlayerInfoListRemoveItem(const FString& Sn, bool IsDaTingPlayer)
{
	if (IsDaTingPlayer)
	{
		// 循环查找名字匹配的玩家信息
		for (int32 i = 0; i < DaTingPlayers.Num(); i++)
		{
			if (DaTingPlayers[i].SN == Sn)
			{
				UE_LOG(LogTemp, Log, TEXT("从大厅玩家数组里移除%s玩家"), *DaTingPlayers[i].SN);
				DaTingPlayers.RemoveAt(i);
				break; // 命中后退出循环
			}
		}
	}
	else
	{
		// 循环查找名字匹配的玩家信息
		for (int32 i = 0; i < RoomDSPlayers.Num(); i++)
		{
			if (RoomDSPlayers[i].SN == Sn)
			{
				UE_LOG(LogTemp, Log, TEXT("从房间DS玩家数组里移除%s玩家"), *RoomDSPlayers[i].SN);
				RoomDSPlayers.RemoveAt(i);
				break; // 命中后退出循环
			}
		}
	}
}
