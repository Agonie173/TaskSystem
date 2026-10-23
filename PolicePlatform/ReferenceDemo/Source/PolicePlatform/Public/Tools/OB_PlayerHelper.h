#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Settings/Data.h"
#include "OB_PlayerHelper.generated.h"

class AHallPlayerController;

/*
 *每一个房间DS里的玩家生成一个该object，用来保存该玩家的一些训练数据，任务用时计时等功能
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom))
class POLICEPLATFORM_API UOB_PlayerHelper : public UObject
{
    GENERATED_BODY()

public:
    UOB_PlayerHelper();

    //实时播报事件
    UPROPERTY(BlueprintReadOnly)
    TArray<FTaskResult> TaskScoreArray;

    //所有任务是否已经结束
    UPROPERTY(BlueprintReadOnly)
    bool bAllTaskEnd = false;
    
    //任务用时
    UPROPERTY(BlueprintReadOnly)
    int32 TotalTaskTime = 0;

    UFUNCTION()
    void StartTaskTimer();

    UFUNCTION()
    void StopTaskTimer();

    //暂停训练
    UFUNCTION()
    void PauseTrain();

    //恢复训练
    UFUNCTION()
    void ResumeTrain();

    //创建时初始化玩家信息和大厅控制器引用
    void Init(const FString& InSN, int32 InRoomID, AHallPlayerController* InController);

    virtual void BeginDestroy() override;

private:
    FTimerHandle TimerHandle;

    UFUNCTION()
    void OnTimerTick();

    //训练时间以教练端为准，所以每隔几秒发送信息到房间DS校准下训练时间
    void SendCalibrationMessage();

    UPROPERTY()
    AHallPlayerController* JLD_PC = nullptr;

    FString PlayerSN = TEXT("");
    int32 PlayerRoomID = -1;
};
