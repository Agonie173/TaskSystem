#pragma once

#include "CoreMinimal.h"
#include "GamePlay/PPPlayerControllerBase.h"
#include "KJGameMode.h"
#include "Settings/Data.h"
#include "Component/AC_XSL.h"
#include "KJPlayerController.generated.h"

class APPPlayerBase;


/**
 * 课件玩家控制器基础类
 */
UCLASS()
class POLICEPLATFORM_API AKJPlayerController : public APPPlayerControllerBase
{
	GENERATED_BODY()
	
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	
	
public:
	
	//训练模式还是考核模式
	UPROPERTY()
	bool bIsXunLianMode = true;
	
	//重新体验，只在本地端有效，用于控制退出时是否卸载课件
	UPROPERTY()
	bool bRestart = false;
	
	UPROPERTY()
	class AKJGameMode* GKJGameMode = nullptr;

	UPROPERTY()
	class AKJGameState* GKJGameState = nullptr;
	
	//挂在像素流端控制器下的组件，用来远端RPC用
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务 | 动态组件配置")
	TSubclassOf<UAC_XSL> XSLComponentClass;
	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "警务 | 动态组件")
	UAC_XSL* XSLComponent = nullptr;
	
	//RPC客户端安全开始
	UFUNCTION(Client, Reliable)
	void RPC_ClientStart(EProcessType LProcessType, const FPlayerInfo& LPlayerInfo);
	
	//非玩家客户端退出
	UFUNCTION(Client, Reliable)
	void RPC_ClientExit(bool bForce);
	
	//RPC客户端执行返回大厅
	UFUNCTION(Client, Reliable, BlueprintCallable, Category = "警务|课件控制器")
	void RPC_ClientGoToHall();
	
	//RPC客户端执行重新开始体验
	UFUNCTION(Client, Reliable, BlueprintCallable, Category = "警务|课件控制器")
	void RPC_ClientRestartTrain(int32 LPort);
	
	//像素流端通知从房间DS中踢出某个学员
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "警务|课件控制器")
	void RPC_ServerKickPlayer(const FString& LSn);
	
	//像素流端通知某个玩家重新开始体验
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "警务|课件控制器")
	void RPC_ServerPlayerRestart(const FString& LSn);
	
	//像素流端通知某个玩家暂停训练
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "警务|课件控制器")
	void RPC_ServerPauseTrain(bool LPause, const FString& LSn);
	
	//服务器上发送ws通知中心DS像素流客户端正在看谁
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "警务|课件控制器")
	void RPC_ServerNotiyLookWho(const FString& LSN);

	//服务器调用，客户端执行，通知教练端当前观看的玩家变化
	UFUNCTION(Client, Reliable, BlueprintCallable, Category = "警务|课件控制器")
	void RPC_ClientNotifyJLDLookWho(APPPlayerBase* LLookWho);

	//返回大厅
	UFUNCTION(BlueprintCallable, Category = "警务|课件控制器")
	void GoToHall();
};