#pragma once

#include "CoreMinimal.h"
#include "GamePlay/PPGameModeBase.h"
#include "HallGameMode.generated.h"




class AHallGameState;
class UPPGameInstance;
class UInfoSubsystem;
class AHallPlayerController;

/**
 *大厅
 */
UCLASS()
class POLICEPLATFORM_API AHallGameMode : public APPGameModeBase
{
	GENERATED_BODY()
	
public:
	
	// 构造函数
	AHallGameMode();
	
	// 缓存的大厅游戏状态
	UPROPERTY()
	AHallGameState* HallGameState = nullptr;
	// 缓存的PP游戏实例
	UPROPERTY()
	UPPGameInstance* PPGameInstance = nullptr;
	//信息子系统
	UPROPERTY()
	UInfoSubsystem* InfoSubsystem = nullptr;
	//保存教练端PC引用
	UPROPERTY()
	AHallPlayerController* JLD_PC = nullptr;

	
	
	// 房间DS待连接玩家字典：RoomID -> 玩家SN数组
	TMap<int32, TArray<FString>> RoomWaitingPlayers;
	
	// 房间DS待连接玩家选择的训练模式还是考核模式：RoomID -> 模式
	TMap<int32, bool> RoomWaitingPlayersMode;
	
	
	//玩家进入大厅（第一次进入大厅/从房间DS返回）
	UFUNCTION()
	void PlayerIntoHallDS(AHallPlayerController* PC);
	
	// 通知玩家进入房间DS
	UFUNCTION()
	void NotifyPlayerIntoRoomDS(int32 RoomDSPort, int32 MapID, int32 RoomID, bool IsXunLianMode, const TArray<FString>& Sn);
	


	
protected:
	virtual void BeginPlay() override;

	// 根据参数判断客户端
	virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	
	// 玩家完全登录后调用，此时可安全发送Client RPC
	virtual void PostLogin(APlayerController* NewPlayer) override;
	
	// 玩家登出时调用，教练端关闭时把大厅DS服务器也关闭掉
	virtual void Logout(AController* Exiting) override;
};
