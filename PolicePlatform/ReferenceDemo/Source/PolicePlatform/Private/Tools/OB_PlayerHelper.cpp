#include "Tools/OB_PlayerHelper.h"
#include "GamePlay/HallPlayerController.h"
#include "TimerManager.h"

UOB_PlayerHelper::UOB_PlayerHelper()
{
}

void UOB_PlayerHelper::StartTaskTimer()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UOB_PlayerHelper::OnTimerTick, 1.0f, true);
    }
}

void UOB_PlayerHelper::StopTaskTimer()
{
    bAllTaskEnd = true;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
    }

    // 发送最后一次校准
    SendCalibrationMessage();
}

void UOB_PlayerHelper::PauseTrain()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().PauseTimer(TimerHandle);
    }

    // 发送暂停时的校准
    SendCalibrationMessage();
}

void UOB_PlayerHelper::ResumeTrain()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().UnPauseTimer(TimerHandle);
    }

   
}

void UOB_PlayerHelper::OnTimerTick()
{
    TotalTaskTime++;

    // 每5秒发送一次校准消息
    if (TotalTaskTime % 5 == 0)
    {
        SendCalibrationMessage();
    }
}


void UOB_PlayerHelper::SendCalibrationMessage()
{
    if (JLD_PC && !PlayerSN.IsEmpty())
    {
        JLD_PC->RPC_ServerJLDSendCalibrationMessage(PlayerSN, PlayerRoomID, TotalTaskTime);
    }
}

void UOB_PlayerHelper::Init(const FString& InSN, int32 InRoomID, AHallPlayerController* InController)
{
    PlayerSN = InSN;
    PlayerRoomID = InRoomID;
    JLD_PC = InController;
}

void UOB_PlayerHelper::BeginDestroy()
{
    StopTaskTimer();
    Super::BeginDestroy();
}
