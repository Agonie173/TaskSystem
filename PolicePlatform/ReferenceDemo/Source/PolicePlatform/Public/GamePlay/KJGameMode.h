#pragma once

#include "CoreMinimal.h"
#include "GamePlay/PPGameModeBase.h"
#include "Settings/Data.h"
#include "KJGameMode.generated.h"

class AKJPlayerController;
/**
 * 课件gamemode基础类
 */
UCLASS()
class POLICEPLATFORM_API AKJGameMode : public APPGameModeBase
{
	GENERATED_BODY()
	
	virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* ExitingController) override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	
public:
	virtual void BeginPlay() override;
	
	UPROPERTY(BlueprintReadOnly, Category="警务")
	FRoomDSInfo GRoomDSInfo;
	
	//重新开始体验的玩家编号
	UPROPERTY()
	FString RestartPlayerSN = TEXT("");
	
	//把学员从当前房间DS中踢出
	UFUNCTION()
	void KickPlayer(const FString& LSN);
	
	
	//初始化控制器里的玩家信息
	UFUNCTION()
	void SetPCGPlayerInfo(const FString& Options, AKJPlayerController* LKJPC);
	
	//初始化像素流控制器里的玩家信息
	UFUNCTION()
	void SetXSLPCGPlayerInfo(AKJPlayerController* LKJPC);
	
	//检查是否应该关闭当前房间DS
	UFUNCTION()
	void CheckNeedCloseRoomDS() const;
	
	//DS时区分不同的客户端初始化
	UFUNCTION()
	void DSClientTypeInit(const FString& LOptions, const FString& LClientType, AKJPlayerController* LKJPC);
	
	//像素流端控制器
	UPROPERTY(BlueprintReadOnly, Category="警务")
	AKJPlayerController* XSL_PlayerController = nullptr;
	
	//教练端观看视口的玩家变化
	UFUNCTION()
	void JLDLookWhoChange(const FString& LSN);

	// 自动设置或者恢复像素流默认监控学员
	void TrySetDefaultMonitoredStudent();
	
	//教练端中断所有训练，踢出所有玩家
	UFUNCTION()
	void KillAllPlayer();

	//根据玩家SN校准训练用时
	UFUNCTION()
	void CalibrateTrainTime(const FString& LSn, int32 LRoomID, int32 LTrainTime);
	
};