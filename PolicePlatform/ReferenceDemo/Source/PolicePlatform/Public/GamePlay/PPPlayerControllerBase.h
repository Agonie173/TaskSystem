#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Settings/Data.h"
#include "PPPlayerControllerBase.generated.h"


/*
 *控制器基类
 */

class UInfoSubsystem;
class UWSSubsystem;

UCLASS()
class POLICEPLATFORM_API APPPlayerControllerBase : public APlayerController
{
	GENERATED_BODY()

private:
	UPROPERTY()
	UWSSubsystem*  GWSSubsystem = nullptr;
	
	UPROPERTY()
	UInfoSubsystem*  GInfoSubsystem = nullptr;
	
	
public:
	APPPlayerControllerBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	//大厅DS同步玩家状态变化用
	UFUNCTION()
	virtual void NotifyPlayerStatusChange(FPlayerStatus LPlayerStatus);
	
	UPROPERTY()
	FPlayerInfo GPlayerInfo;
	
	UPROPERTY()
	FTimerHandle TimerHandle_VR_ReportHeadsetBattery;
	
	
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "警务|控制器")
	EProcessType ProcessType = EProcessType::None;
	
	//在编辑器里,如果要测试VR流程就设置该变量为真
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="警务|编辑器测试用")
	bool bTestVR = false;
	
	//在编辑器里,要测试哪种模式
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="警务|编辑器测试用")
	bool bTestIsXunLianMode = true;
	
	UFUNCTION()
	UWSSubsystem* GetWSSubsystem();
	
	UFUNCTION()
	UInfoSubsystem* GetInfoSubsystem();
	
	//服务器端开始执行安全入口，蓝图实现
	UFUNCTION(BlueprintImplementableEvent, Category = "警务|课件控制器")
	void ServerStart_BIE(EProcessType LProcessType, const FPlayerInfo& LPlayerInfo);
	
	//客户端执行逻辑安全入口，蓝图实现
	UFUNCTION(BlueprintImplementableEvent, Category = "警务|课件控制器")
	void ClientStart_BIE(EProcessType LProcessType, const FPlayerInfo& LPlayerInfo);
	
	//RPC服务器玩家状态变化
	UFUNCTION(Server, Unreliable, BlueprintCallable, Category = "警务|课件控制器")
	void RPC_ServerNotifyPlayerStatusChange(bool IsHallPlayer, FPlayerStatus LPlayerStatus);
	
	//VR学员端上报电量信息
	UFUNCTION(BlueprintCallable, Category = "警务|课件控制器")
	void VR_ReportBattery(bool LIsHallPlayer);
	
	//编辑器里测试初始化学员类型
	UFUNCTION()
	void EditorInit();
	
	//单机模式初始化学员类型
	UFUNCTION()
	void StandaloneInit();
};
