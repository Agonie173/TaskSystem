#include "Tools/TaskItem.h"

#include "GamePlay/KJGameMode.h"
#include "GamePlay/PPPlayerBase.h"

UTaskItem::UTaskItem()
{
}


void UTaskItem::EndTask(ETaskResult LResultType, const FString& LYYSpeak, const FString& LRecordText)
{
    //先在服务器上广播
    if (GetWorld() && GetWorld()->GetNetMode() < NM_ListenServer)
    {
        FTaskResult LTaskResult;
        
        LTaskResult.SN =OwnerPlayer->GPlayerInfo.SN;
        LTaskResult.RoomID = OwnerPlayer->GPlayerInfo.RoomID;
        LTaskResult.MapID = OwnerPlayer->GPlayerInfo.MapID;
        LTaskResult.TaskName = TaskData.TaskName;
        LTaskResult.Keywords = TaskData.Keywords;
        LTaskResult.TaskResult = LResultType;
        LTaskResult.YYSpeak = LYYSpeak;
        LTaskResult.RecordText = LRecordText;
    
        switch (LResultType)
        {
        case ETaskResult::Succeed:
            LTaskResult.Score = TaskData.SucceedScore;
            break;
        
        case ETaskResult::Fail:
            LTaskResult.Score = TaskData.FailScore;
            break;
        
        case ETaskResult::TimeOut:
            LTaskResult.Score = TaskData.TimeoutScore;
            break;
        
        case ETaskResult::IsGetOutOfLine:
            LTaskResult.Score = TaskData.FailScore;
            break;
        default:
            UE_LOG(LogTemp, Warning, TEXT("EndTask 给的任务结果为枚举none"));
       
        }
        
        OnTaskCompleted.Broadcast(LTaskResult);
    }else
    {
        UE_LOG(LogTemp, Error, TEXT("错误 %s 只应该在服务器上调用EndTask"), *TaskData.TaskName);
    }
}
