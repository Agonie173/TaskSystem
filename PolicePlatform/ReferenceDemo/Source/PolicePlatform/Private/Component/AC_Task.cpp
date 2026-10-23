#include "Component/AC_Task.h"
#include "GamePlay/PPPlayerBase.h"
#include "GamePlay/KJGameMode.h"
#include "GamePlay/KJPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Network/WSSubsystem.h"
#include "Settings/CourseDefinition.h"// 引入课件数据资产类，用于读取课件任务配置
#include "Tools/DataDrivenTaskItem.h"// 引入数据驱动任务类，用于根据任务定义创建运行时任务对象
#include "GameFramework/Actor.h"// 提供AActor的权限判断功能
#include "Tools/InfoSubsystem.h"// 提供当前进程类型判断
#include "Kismet/GameplayStatics.h"// 提供根据Tag查找Actor和完成延迟生成
#include "Engine/GameInstance.h"// 提供GameInstance访问
#include "Interface/CourseLocalActorActionInterface.h"// 提供课程本地Actor动作接口
#include "Interface/CourseLocalActorCommandInterface.h"// 提供课程本地Actor自定义命令接口
#include "Tools/CourseNPCPathActor.h"// 提供通用NPC路线Actor
#include "Component/CourseNPCPathMovementComponent.h"// 提供Actor沿Spline路线移动的通用组件
#include "GameFramework/PlayerController.h"// 提供PC输入模式和鼠标控制
#include "GameFramework/CharacterMovementComponent.h"// 提供暂停和恢复角色移动所需的移动模式接口

UAC_Task::UAC_Task()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UAC_Task::BeginPlay()
{
    Super::BeginPlay();
    
    OwnerPlayer = Cast<APPPlayerBase>(GetOwner());
    
    //这里延时是为了保证玩家里的是否是训练模式变量网络复制到客户端
    //TODO 这里如果是单人训练，其实在其他玩家端可以不用生成本玩家的任务object，因为如果生成太多object会不会影响性能
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UAC_Task::SpawnTaskObject, 0.5f, false);
}

void UAC_Task::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 玩家离开课件时清理其本地场景角色
    ClearCourseLocalActors();

    // 清理任务实例
    for (UTaskItem* TaskInstance : RuntimeTaskInstances)
    {
        if (TaskInstance)
        {
            // 断开引用环
            TaskInstance->OwnerPlayer = nullptr;
            TaskInstance->OwnerAC_Task = nullptr;
            // 手动销毁对象
            TaskInstance->ConditionalBeginDestroy();
            TaskInstance = nullptr;
        }
    }
    RuntimeTaskInstances.Empty();

    //清除训练用时计时器
    GetWorld()->GetTimerManager().ClearTimer(TrainTimeTimerHandle);

    GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
    
    Super::EndPlay(EndPlayReason);
    
}

void UAC_Task::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UAC_Task, CurrentTaskIndex);
    DOREPLIFETIME(UAC_Task, bIsRunning);
    DOREPLIFETIME(UAC_Task, OwnerPlayer);
    DOREPLIFETIME(UAC_Task, TaskResultArray);
    DOREPLIFETIME(UAC_Task, bIsGetOutOfLine);
    DOREPLIFETIME(UAC_Task, bTrainPaused);
    DOREPLIFETIME(UAC_Task, TrainTime);

    // 将当前整数选择复制给所有能够看到所属玩家的客户端
    DOREPLIFETIME(UAC_Task, CurrentIntegerSelection);

    // 将当前学员已经启用的课程装备同步到相关客户端
    DOREPLIFETIME(UAC_Task, ActiveCourseEquipmentIds);

    // 将装备选择UI权限同步给学员端和像素流观察端
    DOREPLIFETIME(UAC_Task, bCourseEquipmentSelectionUIAllowed);
}

void UAC_Task::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAC_Task::StartTasks()
{
    
    if (RuntimeTaskInstances.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("任务列表为空，无法开始任务！"));
        return;
    }
    
    if (GetWorld() && GetWorld()->GetNetMode() < NM_ListenServer)
    {
        // 清空上一轮训练可能保留的课程装备状态
        ActiveCourseEquipmentIds.Reset();

        // 获取任务组件所属玩家
        AActor* ComponentOwner = GetOwner();

        // 强制尽快把清空后的装备状态同步到客户端
        if (IsValid(ComponentOwner))
        {
            ComponentOwner->ForceNetUpdate();
        }

        // 标记任务流程已经开始
        bIsRunning = true;

        // 重置当前任务索引
        CurrentTaskIndex = -1;

        //开始训练用时计时，1秒一次
        TrainTime = 0;
        GetWorld()->GetTimerManager().SetTimer(TrainTimeTimerHandle, [this]()
        {
            if (!IsValid(this)) return;
            TrainTime = TrainTime+1;
        }, 1.0f, true);

#if WITH_EDITOR

        //编辑器下 从第一个bTestStart值为true的任务开始执行 方便测试
        for (int32 i = 0; i < RuntimeTaskInstances.Num(); i++)
        {
            if (RuntimeTaskInstances[i] && RuntimeTaskInstances[i]->bTestStart)
            {
                CurrentTaskIndex = i - 1;
                break;
            }
        }
        
#endif
        
        //联网时要在房间DS上给大厅DS发送任务开始ws消息
        if (GetWorld()->GetNetMode() == NM_DedicatedServer)
        {
            UWSSubsystem* LWSSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UWSSubsystem>();
            check(LWSSubsystem);
            LWSSubsystem->WS_SendTaskStartEndToCenterDS(true, OwnerPlayer->GPlayerInfo.SN, OwnerPlayer->GPlayerInfo.RoomID);
        }
        
        StartNextTask(FTaskResult());
    }
    
}

void UAC_Task::ReportCourseEvent(FName EventId, FName TargetId)
{
    // 创建一条完整的课程事件数据
    FCourseEventData EventData;

    // 保存玩家完成的操作ID
    EventData.EventId = EventId;

    // 保存玩家操作的目标ID
    EventData.TargetId = TargetId;

    // 标记当前普通事件没有携带整数
    EventData.bHasIntegerValue = false;

    // 使用统一入口处理当前课程事件
    SubmitCourseEvent(EventData);
}

// 根据当前网络权限直接处理事件或将事件发送到服务器
void UAC_Task::SubmitCourseEvent(const FCourseEventData& EventData)
{
    // 获取当前任务组件所属的Actor
    AActor* ComponentOwner = GetOwner();

    // 判断任务组件是否拥有有效的所属Actor
    if (!IsValid(ComponentOwner))
    {
        // 输出错误日志，说明无法判断当前网络权限
        UE_LOG(LogTemp, Error, TEXT("课程事件上报失败：任务组件Owner无效"));
        return;
    }

    // 客户端和单机在暂停期间不发送课程事件
    if (bTrainPaused)
    {
        UE_LOG(LogTemp, Warning, TEXT("课程事件上报已忽略：训练处于暂停状态，事件=%s，目标=%s"), *EventData.EventId.ToString(), *EventData.TargetId.ToString());
        return;
    }

    // 服务器产生的事件不需要经过RPC，直接执行服务器处理逻辑
    if (ComponentOwner->HasAuthority())
    {
        // 在服务器上处理当前课程事件
        HandleCourseEvent(EventData);
        return;
    }

    // 客户端通过可靠RPC把当前课程事件发送给服务器
    RPC_ServerReportCourseEvent(EventData);
}

void UAC_Task::HandleCourseSpeechText(int32 SpeechTaskIndex, const FString& RecognizedText)
{
    // 服务器拒绝暂停期间到达的语音识别结果
    if (bTrainPaused)
    {
        UE_LOG(LogTemp, Warning, TEXT("服务器拒绝暂停期间的课程语音结果：任务索引=%d"), SpeechTaskIndex);
        return;
    }

    // 拒绝已经过期的语音插件回调
    if (SpeechTaskIndex != CurrentTaskIndex)
    {
        UE_LOG(LogTemp, Warning, TEXT("服务器拒绝过期语音结果：上报任务索引=%d，当前任务索引=%d"), SpeechTaskIndex, CurrentTaskIndex);
        return;
    }

    // 判断当前任务索引是否有效
    if (!RuntimeTaskInstances.IsValidIndex(CurrentTaskIndex))
    {
        return;
    }

    // 获取当前数据驱动任务
    UDataDrivenTaskItem* DataDrivenTaskInstance = Cast<UDataDrivenTaskItem>(RuntimeTaskInstances[CurrentTaskIndex]);

    // 当前任务不是数据驱动任务时不处理
    if (!IsValid(DataDrivenTaskInstance))
    {
        return;
    }

    // 将本次识别文本交给当前语音任务累计判断
    DataDrivenTaskInstance->TryCompleteBySpeechText(RecognizedText);
}

// 为本地PC学员打开成绩页面需要的鼠标和UI输入
void UAC_Task::PrepareLocalPCResultInputMode()
{
    // 只有本地控制的PC学员需要显示成绩页鼠标
    if (!IsValid(OwnerPlayer) || OwnerPlayer->ProcessType != EProcessType::PC_XYD || !OwnerPlayer->IsLocallyControlled())
    {
        return;
    }

    // 获取当前PC学员的本地玩家控制器
    APlayerController* PlayerController = Cast<APlayerController>(OwnerPlayer->GetController());

    // 玩家控制器无效时不能修改输入模式
    if (!IsValid(PlayerController))
    {
        UE_LOG(LogTemp, Error, TEXT("PC成绩页面输入模式设置失败：PlayerController无效"));
        return;
    }

    // 创建允许游戏和成绩UI共同接收输入的模式
    FInputModeGameAndUI InputMode;

    // 不把鼠标锁定在游戏视口中
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

    // 点击成绩UI时不隐藏鼠标
    InputMode.SetHideCursorDuringCapture(false);

    // 应用PC成绩页面输入模式
    PlayerController->SetInputMode(InputMode);

    // 显示PC鼠标光标
    PlayerController->bShowMouseCursor = true;

    // 输出设置完成日志
    UE_LOG(LogTemp, Warning, TEXT("PC成绩页面输入模式设置完成：学员=%s"), *OwnerPlayer->GetName());
}

void UAC_Task::RPC_ServerReportCourseSpeechText_Implementation(int32 SpeechTaskIndex, const FString& RecognizedText)
{
    // 交给服务器统一处理
    HandleCourseSpeechText(SpeechTaskIndex, RecognizedText);
}

// 上报携带整数的课程事件
void UAC_Task::ReportCourseEventWithInteger(FName EventId, FName TargetId, int32 IntegerValue)
{
    // 创建一条完整的课程事件数据
    FCourseEventData EventData;

    // 保存玩家完成的操作ID
    EventData.EventId = EventId;

    // 保存玩家操作的目标ID
    EventData.TargetId = TargetId;

    // 标记当前事件携带有效整数，数值为0时仍然有效
    EventData.bHasIntegerValue = true;

    // 保存玩家选择的整数，本任务中表示协同警员数量
    EventData.IntegerValue = IntegerValue;

    // 输出整数事件上报日志，方便检查UI选择结果
    //UE_LOG(LogTemp, Warning, TEXT("课程整数事件上报：事件=%s，目标=%s，整数=%d"), *EventData.EventId.ToString(), *EventData.TargetId.ToString(), EventData.IntegerValue);

    // 使用统一入口处理当前课程事件
    SubmitCourseEvent(EventData);
}

void UAC_Task::StartNextTask(FTaskResult LPreviousTaskResult)
{
    if (!bIsRunning)
        return;
    
    if (GetWorld() && GetWorld()->GetNetMode() < NM_ListenServer)
    {
        
        CurrentTaskIndex = CurrentTaskIndex + 1;
        
        
        if (CurrentTaskIndex < RuntimeTaskInstances.Num())
        {
            // 重置上一项选择，-1表示玩家尚未选择
            CurrentIntegerSelection = -1;

            // 通知服务器本地UI清除旧选择
            OnCourseIntegerSelectionChanged.Broadcast(CurrentIntegerSelection);

            // 获取任务组件所属Actor
            AActor* ComponentOwner = GetOwner();

            // 让重置后的选择尽快复制到客户端
            if (IsValid(ComponentOwner))
            {
                ComponentOwner->ForceNetUpdate();
            }

            // 获取即将开始的新任务
            UTaskItem* NewTask = RuntimeTaskInstances[CurrentTaskIndex];
            // 绑定新任务完成事件
            NewTask->OnTaskCompleted.AddDynamic(this, &UAC_Task::StartNextTask);
            
        }
        else if (CurrentTaskIndex >= RuntimeTaskInstances.Num())
        {
            bIsRunning = false;
            UE_LOG(LogTemp, Log, TEXT("玩家:%s 所有任务都结束了"), *OwnerPlayer->GetName());
            
            //联网时要在房间DS上给大厅DS发送任务结束ws消息
            if (GetWorld()->GetNetMode() == NM_DedicatedServer)
            {
                UWSSubsystem* LWSSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UWSSubsystem>();
                check(LWSSubsystem);
                LWSSubsystem->WS_SendTaskStartEndToCenterDS(false, OwnerPlayer->GPlayerInfo.SN, OwnerPlayer->GPlayerInfo.RoomID);
            }
           
        }

        RPC_MulticastStartNewTask(CurrentTaskIndex, LPreviousTaskResult);
        
    }
   
}

UTaskItem* UAC_Task::GetCurrentTask() const
{
    return CurrentTask;
    
}


void UAC_Task::OnRep_CurrentTaskIndex()
{
}

void UAC_Task::OnRep_TrainTime()
{
    OnTrainTimeChanged.Broadcast(TrainTime);
}

// 客户端收到服务器同步的暂停状态
void UAC_Task::OnRep_TrainPaused()
{
    // 在当前客户端统一应用最新暂停状态
    ApplyTrainPausedState();
}

void UAC_Task::RPC_ServerEndTrain_Implementation()
{
    if (!bIsRunning)
    {
        return;
    }
    
    bIsRunning = false;
    bIsGetOutOfLine = true;
    UE_LOG(LogTemp, Log, TEXT("玩家:%s 违规操作 任务提前结束了"), *OwnerPlayer->GetName());
    
    //联网时要在房间DS上给大厅DS发送任务结束ws消息
    if (GetWorld()->GetNetMode() == NM_DedicatedServer)
    {
        UWSSubsystem* LWSSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UWSSubsystem>();
        check(LWSSubsystem);
        LWSSubsystem->WS_SendTaskStartEndToCenterDS(false, OwnerPlayer->GPlayerInfo.SN, OwnerPlayer->GPlayerInfo.RoomID);
    }
    
    //处理未完成的关键考核任务，设置为全部失败
    for (int32 i = CurrentTaskIndex; i < RuntimeTaskInstances.Num(); ++i)
    {
        UTaskItem* Task = RuntimeTaskInstances[i];
        if (Task && Task->IsKeyTask)
        {
            FTaskResult LTaskResult;
            LTaskResult.SN = OwnerPlayer->GPlayerInfo.SN;
            LTaskResult.RoomID = OwnerPlayer->GPlayerInfo.RoomID;
            LTaskResult.MapID = OwnerPlayer->GPlayerInfo.MapID;
            LTaskResult.TaskName = Task->TaskData.TaskName;
            LTaskResult.Keywords = Task->TaskData.Keywords;
            LTaskResult.TaskResult = ETaskResult::Fail;
            LTaskResult.Score = Task->TaskData.FailScore;
            LTaskResult.TaskIndex = TaskResultArray.Num() + 1;
            TaskResultArray.Add(LTaskResult);

            /*
             *
            if (GetWorld()->GetNetMode() == NM_DedicatedServer)
            {
                UWSSubsystem* LWSSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UWSSubsystem>();
                check(LWSSubsystem);
                LWSSubsystem->WS_SendTaskScoreToCenterDS(LTaskResult);
            }
            
            */
        }
    }
    
    //设置CurrentTaskIndex比任务数组大，这样广播下去走的就是任务结束逻辑
    CurrentTaskIndex =  RuntimeTaskInstances.Num() + 1;
    FTaskResult LTaskResult;
    RPC_MulticastStartNewTask(CurrentTaskIndex, LTaskResult);
    
}

void UAC_Task::RPC_ServerEndCurrentTask_Implementation(ETaskResult LResultType, const FString& LYYSpeak, const FString& LRecordText, const FString& LTaskName)
{
    // 服务器拒绝暂停期间直接结束当前任务
    if (bTrainPaused)
    {
        UE_LOG(LogTemp, Warning, TEXT("服务器拒绝暂停期间结束当前任务：任务=%s"), *LTaskName);
        return;
    }

    //对比任务名称，保证结束的是对应的任务
    if (GetCurrentTask() && GetCurrentTask()->TaskData.TaskName == LTaskName)
    {
        GetCurrentTask()->EndTask(LResultType, LYYSpeak, LRecordText);
    }
   
}

// 服务器设置目标学员的训练暂停状态
void UAC_Task::SetTrainPaused(bool bPause)
{
    // 获取当前任务组件所属的玩家Actor
    AActor* ComponentOwner = GetOwner();

    // 只有服务器才能修改权威暂停状态
    if (!IsValid(ComponentOwner) || !ComponentOwner->HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("设置训练暂停状态失败：任务组件Owner无效或当前不是服务器"));
        return;
    }

    // 状态没有变化时不重复暂停计时器、移动和输入
    if (bTrainPaused == bPause)
    {
        UE_LOG(LogTemp, Warning, TEXT("训练暂停状态没有变化：学员=%s，暂停=%s"), *GetNameSafe(OwnerPlayer), bPause ? TEXT("是") : TEXT("否"));
        return;
    }

    // 保存服务器权威暂停状态
    bTrainPaused = bPause;

    // 在服务器本机应用暂停状态
    ApplyTrainPausedState();

    // 要求所属玩家尽快把暂停状态复制给相关客户端
    ComponentOwner->ForceNetUpdate();
}

// 在当前机器统一应用训练暂停或恢复状态
void UAC_Task::ApplyTrainPausedState()
{
    // 获取当前任务组件所属的玩家Actor
    AActor* ComponentOwner = GetOwner();

    // Owner无效时不能继续处理
    if (!IsValid(ComponentOwner))
    {
        UE_LOG(LogTemp, Error, TEXT("应用训练暂停状态失败：任务组件Owner无效"));
        return;
    }

    // 服务器负责暂停或恢复权威训练计时器
    if (ComponentOwner->HasAuthority() && IsValid(GetWorld()))
    {
        // 暂停训练时暂停训练总用时计时器
        if (bTrainPaused)
        {
            GetWorld()->GetTimerManager().PauseTimer(TrainTimeTimerHandle);
        }
        // 恢复训练时继续训练总用时计时器
        else
        {
            GetWorld()->GetTimerManager().UnPauseTimer(TrainTimeTimerHandle);
        }
    }

    // 服务器权威角色和学员自己的本地角色需要应用移动限制
    if (IsValid(OwnerPlayer) && (OwnerPlayer->HasAuthority() || OwnerPlayer->IsLocallyControlled()))
    {
        // 获取角色移动组件
        UCharacterMovementComponent* CharacterMovementComponent = OwnerPlayer->GetCharacterMovement();

        // 移动组件有效时处理暂停或恢复
        if (IsValid(CharacterMovementComponent))
        {
            // 暂停训练时保存移动模式并立即停止角色
            if (bTrainPaused)
            {
                // 只保存一次暂停前的移动模式
                if (!bHasSavedTrainMovementMode && CharacterMovementComponent->MovementMode != MOVE_None)
                {
                    SavedTrainMovementMode = static_cast<uint8>(CharacterMovementComponent->MovementMode);
                    SavedTrainCustomMovementMode = CharacterMovementComponent->CustomMovementMode;
                    bHasSavedTrainMovementMode = true;
                }

                // 清除暂停前尚未消耗的速度
                CharacterMovementComponent->StopMovementImmediately();

                // 把移动模式切换为MOVE_None，禁止继续产生角色移动
                CharacterMovementComponent->DisableMovement();
            }
            // 恢复训练时还原暂停前的移动模式
            else if (bHasSavedTrainMovementMode)
            {
                CharacterMovementComponent->SetMovementMode(static_cast<EMovementMode>(SavedTrainMovementMode), SavedTrainCustomMovementMode);
                bHasSavedTrainMovementMode = false;
            }
        }
    }

    // 只有目标学员自己的本地控制器需要忽略或恢复移动输入
    if (IsValid(OwnerPlayer) && OwnerPlayer->IsLocallyControlled())
    {
        // 获取目标学员自己的本地PlayerController
        APlayerController* LocalPlayerController = Cast<APlayerController>(OwnerPlayer->GetController());

        // 防止影响服务器控制器或其他学员的控制器
        if (IsValid(LocalPlayerController) && LocalPlayerController->IsLocalController())
        {
            LocalPlayerController->SetIgnoreMoveInput(bTrainPaused);
        }
    }

    // 保留原有蓝图委托，继续处理遮罩、声音和Mapping Context
    OnTrainPaused.Broadcast(bTrainPaused);

    // 输出各端实际应用暂停状态的日志
    UE_LOG(LogTemp, Warning, TEXT("训练暂停状态已应用：学员=%s，暂停=%s，服务器权限=%s，本地控制=%s"), *GetNameSafe(OwnerPlayer), bTrainPaused ? TEXT("是") : TEXT("否"), ComponentOwner->HasAuthority() ? TEXT("是") : TEXT("否"), IsValid(OwnerPlayer) && OwnerPlayer->IsLocallyControlled() ? TEXT("是") : TEXT("否"));
}


void UAC_Task::HandleNewTaskStarted(int32 TaskIndex, FTaskResult PreviousTaskResult)
{
    
    int32 LOldIndex = TaskIndex - 1;
    UTaskItem* OldTask = nullptr;
    
    //处理上一个任务的结束事件
    if (LOldIndex >= 0 && LOldIndex < RuntimeTaskInstances.Num())
    {
        OldTask = RuntimeTaskInstances[LOldIndex];
        check(OldTask);
        
        //如果是考核关键任务，就要实时事件播报并记录下后面要成绩结算
        if (OldTask->IsKeyTask)
        {
            //单机或DS服务器上才保存任务成绩
            if (GetWorld()->GetNetMode() < NM_ListenServer)
            {
                PreviousTaskResult.TaskIndex = TaskResultArray.Num() + 1;
                TaskResultArray.Add(PreviousTaskResult);
                
                //考核关键任务需要在联网时在房间DS上给大厅DS发送实时事件播报ws消息
                if (GetWorld()->GetNetMode() == NM_DedicatedServer)
                {
                    UWSSubsystem* LWSSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UWSSubsystem>();
                    check(LWSSubsystem);
                    LWSSubsystem->WS_SendTaskScoreToCenterDS(PreviousTaskResult);
                }
            }
            
        }else if (PreviousTaskResult.TaskResult == ETaskResult::IsGetOutOfLine)
        {
            //TODO目前违规事件走的是任务通道，后期可考虑搞个独立WS通道发送违规实时事件播报
            //联网时在房间DS上给大厅DS发送违规实时事件播报ws消息
            if (GetWorld()->GetNetMode() == NM_DedicatedServer)
            {
                PreviousTaskResult.TaskIndex = TaskResultArray.Num() + 1;
                UWSSubsystem* LWSSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UWSSubsystem>();
                check(LWSSubsystem);
                LWSSubsystem->WS_SendTaskScoreToCenterDS(PreviousTaskResult);
            }
        }
        
        UE_LOG(LogTemp, Log, TEXT("%s 任务结束了"), *OldTask->TaskData.TaskName);
        // 保存上一项任务的执行结果
        OldTask->TaskResult = PreviousTaskResult;

        // 判断上一项任务是否是数据驱动任务
        UDataDrivenTaskItem* OldDataDrivenTaskInstance = Cast<UDataDrivenTaskItem>(OldTask);

        if (IsValid(OldDataDrivenTaskInstance))
        {
            // 数据驱动任务结束时清理任务UI和本地输入
            OldDataDrivenTaskInstance->EndDataDrivenTask();
        }

        // 保留旧系统的蓝图任务结束事件
        OldTask->OnTaskEnd_BIE(PreviousTaskResult);
            
        //客户再广播下，服务器已经在UTaskItem::EndTask广播过了
        if (GetWorld()->GetNetMode() == NM_Client)
        {
            OldTask->OnTaskCompleted.Broadcast(PreviousTaskResult);
        }
    }

    //处理新任务的开始事件
    if (TaskIndex >= 0 && TaskIndex < RuntimeTaskInstances.Num())
    {
        UTaskItem* NewTask = RuntimeTaskInstances[TaskIndex];
        check(NewTask);
        
        CurrentTask = NewTask;
        OnStartNewTask.Broadcast(TaskIndex, PreviousTaskResult);
        
        //有些事情是需要在下个任务开始前处理事情的，所以需要自定义延时时间,默认也要延时下再执行
        float LInterval = 0.6f;
        if (OldTask != nullptr)
        {
            LInterval = FMath::Max(OldTask->NextTaskInterval, 0.6f);
        }
        
        GetWorld()->GetTimerManager().SetTimer(NextTaskTimerHandle, [this, NewTask, LInterval]()
        {
            if (IsValid(this) && IsValid(NewTask) && bIsRunning)
            {
                // 输出当前正式开始执行的任务名称
                UE_LOG(LogTemp, Log, TEXT("开始新任务 %s"), *NewTask->TaskData.TaskName);

                // 判断当前任务是否是新的数据驱动任务
                UDataDrivenTaskItem* DataDrivenTaskInstance = Cast<UDataDrivenTaskItem>(NewTask);

                if (IsValid(DataDrivenTaskInstance))
                {
                    // 数据驱动任务使用C++开始入口
                    DataDrivenTaskInstance->StartDataDrivenTask();
                }
                else
                {
                    // 原有蓝图任务继续调用蓝图开始事件
                    NewTask->OnTaskStart_BIE();
                }
            }
        }, LInterval, false);
      
    }
    else
    {
        //所有任务都结束了
        UE_LOG(LogTemp, Log, TEXT("所有任务结束"));
        //这是需要的，不然可能会出现当违规使用了警械 任务流程已经结束，但还在触发新任务的情况
        GetWorld()->GetTimerManager().ClearTimer(NextTaskTimerHandle);

        //清除训练用时计时器
        GetWorld()->GetTimerManager().ClearTimer(TrainTimeTimerHandle);
        
        float LInterval = 2.0f;
        if (OldTask != nullptr)
        {
            LInterval = FMath::Max(OldTask->NextTaskInterval, 2.0f);
        }
        
        //延时下再广播任务结束，保证任务成绩数组成功赋值到客户端
        FTimerHandle TimerHandle;
        /*GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
        {
             if (!IsValid(this)) return;
            OnAllTasksCompleted.Broadcast();
        }, LInterval, false);*/
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
            {
                // 任务组件已经失效时不继续执行
                if (!IsValid(this))
                {
                    return;
                }

                // 在成绩页面创建前为本地PC学员开启鼠标和UI输入
                PrepareLocalPCResultInputMode();

                // 通知蓝图创建最终成绩页面
                OnAllTasksCompleted.Broadcast();
            }, LInterval, false);
       
    }
   
}

void UAC_Task::SpawnTaskObject()
{
    //因为object不会自动复制，所以在所有客户端都生成
    //TODO 后期应该通过json文件数据来初始化任务object,方便实现任务编辑
    //TODO 待优化，其他玩家端其实不需要生成这些任务object
    //TODO生成的这些object是否需要手动销毁？

    // 判断任务组件所属的玩家是否有效，避免后续读取玩家模式时出现空指针
    if (!IsValid(OwnerPlayer))
    {
        // 输出错误信息，说明任务组件没有找到正确的玩家
        UE_LOG(LogTemp,Error,TEXT("生成任务失败：OwnerPlayer无效"));
        return;
    }

    // 配置了课件数据资产时，使用新的数据驱动方式生成任务
    if (IsValid(CourseDefinition))
    {
        // 按照DataAsset中的数组顺序遍历所有任务定义
        for (const FCourseTaskDefinition& TaskDefinition : CourseDefinition->Tasks)
        {
            // 考核模式下跳过只允许在训练模式中执行的任务
            if (!OwnerPlayer->bIsXunLianMode && TaskDefinition.bOnlyXunLianModeValid)
            {
                continue;
            }

            // 创建一条数据驱动运行时任务，并将任务组件作为它的Outer
            UDataDrivenTaskItem* DataDrivenTaskInstance = NewObject<UDataDrivenTaskItem>(this);

            // 判断运行时任务是否创建成功
            if (!IsValid(DataDrivenTaskInstance))
            {
                // 输出创建失败的任务名称，方便定位具体配置
                UE_LOG(LogTemp, Error, TEXT("数据驱动任务创建失败：%s"), *TaskDefinition.TaskData.TaskName);
                continue;
            }

            // 保存当前任务所属的玩家
            DataDrivenTaskInstance->OwnerPlayer = OwnerPlayer;

            // 保存负责管理当前任务的任务组件
            DataDrivenTaskInstance->OwnerAC_Task = this;

            // 将数据资产中的任务定义复制到运行时任务对象
            DataDrivenTaskInstance->InitializeFromDefinition(TaskDefinition);

            // 保留旧任务系统的初始化蓝图事件，方便以后按需扩展蓝图表现
            DataDrivenTaskInstance->OnTaskInit_BIE();

            // 将创建完成的任务加入旧系统使用的运行时任务数组
            RuntimeTaskInstances.Add(DataDrivenTaskInstance);
        }

        // 输出配置数量和实际生成数量，验证训练、考核模式的任务过滤结果
        UE_LOG(LogTemp, Warning, TEXT("数据驱动任务生成完成：课件=%s，配置数量=%d，实际生成数量=%d"), *CourseDefinition->CourseName, CourseDefinition->Tasks.Num(), RuntimeTaskInstances.Num());

        // 数据驱动任务已经生成，不再继续执行下面的旧任务生成代码
        return;
    }

    for (TSubclassOf<UTaskItem> TaskClass : TaskClassList)
    {
        if (TaskClass)
        {
            UTaskItem* TaskInstance = NewObject<UTaskItem>(this, TaskClass);
            if (OwnerPlayer->bIsXunLianMode)
            {
                TaskInstance->OwnerPlayer = OwnerPlayer;
                TaskInstance->OwnerAC_Task = this;
                TaskInstance->OnTaskInit_BIE();
                RuntimeTaskInstances.Add(TaskInstance);
            }
            else
            {
                //考核模式下仅添加非仅训练模式下有效的任务
                if (!TaskInstance->bOnlyXunLianModeVaild)
                {
                    TaskInstance->OwnerPlayer = OwnerPlayer;
                    TaskInstance->OwnerAC_Task = this;
                    TaskInstance->OnTaskInit_BIE();
                    RuntimeTaskInstances.Add(TaskInstance);
                }
               
            }
           
        }
    }
}

void UAC_Task::RPC_MulticastStartNewTask_Implementation(int32 TaskIndex, FTaskResult PreviousTaskResult)
{
    HandleNewTaskStarted(TaskIndex, PreviousTaskResult);
}

UAC_XSL* UAC_Task::GetAC_XSL()
{
    if (CachedXSLComponent)
    {
        return CachedXSLComponent;
    }
    
    AKJGameMode* GameMode = Cast<AKJGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode->XSL_PlayerController)
    {
        UE_LOG(LogTemp, Error, TEXT("UAC_Task::GetAC_XSL - 当前像素流控制器为空"));
        return nullptr;
    }

    CachedXSLComponent = GameMode->XSL_PlayerController->XSLComponent;

    return CachedXSLComponent;
}

void UAC_Task::ReportCourseIntegerSelection(int32 IntegerValue)
{
    // 暂停期间不允许本地UI修改整数选择
    if (bTrainPaused)
    {
        UE_LOG(LogTemp, Warning, TEXT("课程整数选择已忽略：训练处于暂停状态，整数=%d"), IntegerValue);
        return;
    }

    // 只接受当前出警人数任务允许的选择范围
    if (IntegerValue < 0 || IntegerValue > 3)
    {
        // 输出非法选择日志
        UE_LOG(LogTemp, Error, TEXT("课程整数选择上报失败：整数超出范围，整数=%d"), IntegerValue);
        return;
    }

    // 获取任务组件所属的玩家Actor
    AActor* ComponentOwner = GetOwner();

    // 判断所属玩家是否有效
    if (!IsValid(ComponentOwner))
    {
        // Owner无效时无法继续同步
        UE_LOG(LogTemp, Error, TEXT("课程整数选择上报失败：任务组件Owner无效"));
        return;
    }

    // 如果当前就在服务器上，则直接保存选择
    if (ComponentOwner->HasAuthority())
    {
        // 保存服务器权威选择值
        CurrentIntegerSelection = IntegerValue;

        // 通知服务器本地创建的UI刷新
        OnCourseIntegerSelectionChanged.Broadcast(CurrentIntegerSelection);

        // 强制所属Actor尽快执行网络更新
        ComponentOwner->ForceNetUpdate();
        return;
    }

    // 将本地玩家的选择发送给服务器
    RPC_ServerSetCourseIntegerSelection(IntegerValue);
}

bool UAC_Task::TrySubmitCurrentCourseTarget(FName SelectedTargetId)
{
    // 训练暂停期间不允许提交任何课程目标
    if (bTrainPaused)
    {
        UE_LOG(LogTemp, Warning, TEXT("课程目标提交已忽略：训练处于暂停状态，目标=%s"), *SelectedTargetId.ToString());
        return false;
    }

    // 判断任务流程是否正在运行
    if (!bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("课程目标提交失败：当前任务流程没有运行"));
        return false;
    }

    // 判断玩家是否提交了有效目标ID
    if (SelectedTargetId.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("课程目标提交失败：SelectedTargetId为空"));
        return false;
    }

    // 将当前任务转换成数据驱动任务
    UDataDrivenTaskItem* CurrentDataDrivenTask = Cast<UDataDrivenTaskItem>(CurrentTask);

    // 判断当前任务是否为数据驱动任务
    if (!IsValid(CurrentDataDrivenTask))
    {
        UE_LOG(LogTemp, Warning, TEXT("课程目标提交失败：当前任务不是数据驱动任务"));
        return false;
    }

    // 当前任务已经提交成功并等待结束时不再接受重复提交
    if (!CurrentDataDrivenTask->CanAcceptCourseSubmission())
    {
        UE_LOG(LogTemp, Warning, TEXT("课程目标提交已忽略：当前任务正在等待结束"));
        return false;
    }

    // 获取普通WaitEvent任务或顺序阶段当前等待的事件和目标
    FName ExpectedEventId = NAME_None;
    FName ExpectedTargetId = NAME_None;

    if (!CurrentDataDrivenTask->GetCurrentCourseEventCondition(ExpectedEventId, ExpectedTargetId))
    {
        UE_LOG(LogTemp, Warning, TEXT("课程目标提交失败：当前任务没有可提交的事件条件，任务=%s"), *CurrentDataDrivenTask->Definition.TaskData.TaskName);
        return false;
    }

    // 通用目标提交必须配置有效目标ID
    if (ExpectedTargetId.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("课程目标提交失败：当前完成条件TargetId为空，任务=%s"), *CurrentDataDrivenTask->Definition.TaskData.TaskName);
        return false;
    }

    // 判断玩家选择的目标是否正确
    if (SelectedTargetId != ExpectedTargetId)
    {
        return false;
    }

    // 使用当前阶段或任务的完成条件上报服务器
    ReportCourseEvent(ExpectedEventId, SelectedTargetId);

    return true;
}

FName UAC_Task::GetCurrentCourseTargetId() const
{
    // 获取当前数据驱动任务
    const UDataDrivenTaskItem* CurrentDataDrivenTask = Cast<UDataDrivenTaskItem>(CurrentTask);

    if (!IsValid(CurrentDataDrivenTask))
    {
        return NAME_None;
    }

    // 获取当前任务或当前阶段正在等待的完成条件
    FName CurrentEventId = NAME_None;
    FName CurrentTargetId = NAME_None;

    if (!CurrentDataDrivenTask->GetCurrentCourseEventCondition(CurrentEventId, CurrentTargetId))
    {
        return NAME_None;
    }

    return CurrentTargetId;
}

// 设置当前任务的装备选择UI权限
void UAC_Task::SetCourseEquipmentSelectionUIAllowed(bool bAllowed)
{
    // 获取任务组件所属的Actor
    AActor* ComponentOwner = GetOwner();

    // 所属Actor无效时不能处理权限
    if (!IsValid(ComponentOwner))
    {
        // 输出异常日志
        UE_LOG(LogTemp, Error, TEXT("设置课程装备选择UI权限失败：AC_Task所属Actor无效"));

        // 结束本次设置
        return;
    }

    // 联机状态下只允许服务器修改该权限，客户端等待属性同步
    if (!ComponentOwner->HasAuthority())
    {
        // 结束客户端本地设置
        return;
    }

    // 权限没有变化时不重复通知和同步
    if (bCourseEquipmentSelectionUIAllowed == bAllowed)
    {
        // 结束本次设置
        return;
    }

    // 保存服务器计算出的最新权限
    bCourseEquipmentSelectionUIAllowed = bAllowed;

    // 立即在服务器或单机环境中应用权限变化
    OnRep_CourseEquipmentSelectionUIAllowed();

    // 要求所属Actor尽快把最新权限同步给所属学员客户端
    ComponentOwner->ForceNetUpdate();
}

// 客户端收到装备选择UI权限后应用到本地蓝图
void UAC_Task::OnRep_CourseEquipmentSelectionUIAllowed()
{
    // 通知本地蓝图刷新装备选择UI权限，权限关闭时也可以自动关闭现有UI
    OnCourseEquipmentSelectionPermissionChanged.Broadcast(bCourseEquipmentSelectionUIAllowed);

    // 判断任务所属玩家是否为当前设备上的本地玩家
    const bool bIsLocallyControlledPlayer = IsValid(OwnerPlayer) && OwnerPlayer->IsLocallyControlled();

    // 输出同步结果，便于确认一体机是否真正收到服务器权限
    UE_LOG(LogTemp, Warning, TEXT("课程装备选择UI权限已应用：玩家=%s，允许打开=%s，本地控制=%s"), *GetNameSafe(OwnerPlayer), bCourseEquipmentSelectionUIAllowed ? TEXT("是") : TEXT("否"), bIsLocallyControlledPlayer ? TEXT("是") : TEXT("否"));
}

// 判断当前本地PC或VR学员是否可以使用课程装备选择功能
bool UAC_Task::CanOpenCourseEquipmentSelectionUI() const
{
    // 训练暂停期间不能打开或操作课程装备栏
    if (bTrainPaused)
    {
        return false;
    }

    // 当前任务没有开放装备选择时不能使用
    if (!bCourseEquipmentSelectionUIAllowed)
    {
        return false;
    }

    // 玩家对象无效时不能使用
    if (!IsValid(OwnerPlayer))
    {
        return false;
    }

    // 判断当前任务所属玩家是否为PC或VR学员
    const bool bIsStudentOperationClient = OwnerPlayer->ProcessType == EProcessType::PC_XYD || OwnerPlayer->ProcessType == EProcessType::VR_XYD;

    // 教练端、像素流观察端和其他进程不能使用装备选择功能
    if (!bIsStudentOperationClient)
    {
        return false;
    }

    // 只有当前玩家所属的本地客户端允许使用
    return OwnerPlayer->IsLocallyControlled();
}

// 设置当前学员指定课程装备的启用状态
bool UAC_Task::SetCourseEquipmentActive(FName EquipmentId, bool bActive)
{
    // 装备ID为空时不能修改装备状态
    if (EquipmentId.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("设置课程装备状态失败：EquipmentId为空"));
        return false;
    }

    // 只有当前任务正式开放装备栏时才允许提交装备状态
    if (!CanOpenCourseEquipmentSelectionUI())
    {
        UE_LOG(LogTemp, Warning, TEXT("设置课程装备状态失败：当前本地学员没有装备选择权限，装备=%s"), *EquipmentId.ToString());
        return false;
    }

    // 获取任务组件所属玩家Actor
    AActor* ComponentOwner = GetOwner();

    // 所属玩家Actor无效时不能判断网络权限
    if (!IsValid(ComponentOwner))
    {
        UE_LOG(LogTemp, Error, TEXT("设置课程装备状态失败：任务组件Owner无效，装备=%s"), *EquipmentId.ToString());
        return false;
    }

    // 单机或服务器本地操作直接修改服务器权威状态
    if (ComponentOwner->HasAuthority())
    {
        return ApplyCourseEquipmentState(EquipmentId, bActive);
    }

    // 将当前任务索引和装备状态发送给服务器
    RPC_ServerSetCourseEquipmentActive(CurrentTaskIndex, EquipmentId, bActive);

    // 返回本次装备状态请求已经成功发送
    return true;
}

// 处理一次课程装备选择，并返回装备选择UI是否可以关闭
bool UAC_Task::SubmitCourseEquipmentSelection(FName EquipmentId)
{
    // 先把玩家实际选择的装备保存为服务器权威状态
    if (!SetCourseEquipmentActive(EquipmentId, true))
    {
        UE_LOG(LogTemp, Warning, TEXT("提交课程装备选择失败：装备状态没有被接受，装备=%s"), *EquipmentId.ToString());
        return false;
    }

    // 获取当前正在执行的数据驱动任务
    UDataDrivenTaskItem* CurrentDataDrivenTask = Cast<UDataDrivenTaskItem>(CurrentTask);

    // 旧任务或非数据驱动任务只记录装备状态，不使用装备结束任务
    if (!IsValid(CurrentDataDrivenTask))
    {
        UE_LOG(LogTemp, Warning, TEXT("课程装备选择已记录：当前不是数据驱动任务，装备=%s"), *EquipmentId.ToString());
        return true;
    }

    // 判断当前任务是否将装备选择同时作为任务完成目标
    const bool bEquipmentIsTaskTarget = CurrentDataDrivenTask->Definition.TriggerType == ECourseTaskTriggerType::WaitEvent && !CurrentDataDrivenTask->Definition.TargetId.IsNone();

    // 语音任务等非装备目标任务只记录装备状态，不改变任务进度
    if (!bEquipmentIsTaskTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("课程装备选择已记录但不结束任务：任务=%s，触发类型=%s，装备=%s"), *CurrentDataDrivenTask->Definition.TaskData.TaskName, *UEnum::GetValueAsString(CurrentDataDrivenTask->Definition.TriggerType), *EquipmentId.ToString());
        return true;
    }

    // 训练模式中的装备选择任务继续使用目标ID判断选择是否正确
    const bool bTargetSubmissionSucceeded = TrySubmitCurrentCourseTarget(EquipmentId);

    // 输出本次装备目标任务提交结果
    //UE_LOG(LogTemp, Warning, TEXT("课程装备目标提交结果：任务=%s，装备=%s，提交成功=%s"), *CurrentDataDrivenTask->Definition.TaskData.TaskName, *EquipmentId.ToString(), bTargetSubmissionSucceeded ? TEXT("是") : TEXT("否"));

    // 选择正确时关闭装备栏，选择错误时保留装备栏继续选择
    return bTargetSubmissionSucceeded;
}

// 判断当前学员指定课程装备是否已经启用
bool UAC_Task::IsCourseEquipmentActive(FName EquipmentId) const
{
    // 装备ID为空时统一返回未启用
    if (EquipmentId.IsNone())
    {
        return false;
    }

    // 数组中存在该装备ID时表示装备已经启用
    return ActiveCourseEquipmentIds.Contains(EquipmentId);
}

// 服务器接收所属学员客户端提交的课程装备状态
void UAC_Task::RPC_ServerSetCourseEquipmentActive_Implementation(int32 EquipmentTaskIndex, FName EquipmentId, bool bActive)
{
    // 服务器拒绝暂停期间到达的装备状态请求
    if (bTrainPaused)
    {
        UE_LOG(LogTemp, Warning, TEXT("服务器拒绝暂停期间的课程装备状态：装备=%s，启用=%s"), *EquipmentId.ToString(), bActive ? TEXT("是") : TEXT("否"));
        return;
    }

    // 装备ID为空时拒绝本次请求
    if (EquipmentId.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("服务器拒绝课程装备状态：EquipmentId为空"));
        return;
    }

    // 任务流程没有运行时不能继续修改装备状态
    if (!bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("服务器拒绝课程装备状态：任务流程没有运行，装备=%s"), *EquipmentId.ToString());
        return;
    }

    // 拒绝来自已经结束或切换任务后的旧装备UI请求
    if (EquipmentTaskIndex != CurrentTaskIndex)
    {
        UE_LOG(LogTemp, Warning, TEXT("服务器拒绝过期课程装备状态：上报任务索引=%d，当前任务索引=%d，装备=%s"), EquipmentTaskIndex, CurrentTaskIndex, *EquipmentId.ToString());
        return;
    }

    // 当前服务器任务没有开放装备选择权限时拒绝请求
    if (!bCourseEquipmentSelectionUIAllowed)
    {
        UE_LOG(LogTemp, Warning, TEXT("服务器拒绝课程装备状态：当前任务没有开放装备选择权限，装备=%s"), *EquipmentId.ToString());
        return;
    }

    // 所属玩家无效时不能继续处理
    if (!IsValid(OwnerPlayer))
    {
        UE_LOG(LogTemp, Error, TEXT("服务器拒绝课程装备状态：OwnerPlayer无效，装备=%s"), *EquipmentId.ToString());
        return;
    }

    // 只有PC或VR学员可以提交课程装备状态
    if (OwnerPlayer->ProcessType != EProcessType::PC_XYD && OwnerPlayer->ProcessType != EProcessType::VR_XYD)
    {
        UE_LOG(LogTemp, Warning, TEXT("服务器拒绝课程装备状态：所属玩家不是PC或VR学员，进程=%s，装备=%s"), *UEnum::GetValueAsString(OwnerPlayer->ProcessType), *EquipmentId.ToString());
        return;
    }

    // 通过全部校验后修改服务器权威装备状态
    ApplyCourseEquipmentState(EquipmentId, bActive);
}

// 在服务器或单机中真正修改课程装备状态
bool UAC_Task::ApplyCourseEquipmentState(FName EquipmentId, bool bActive)
{
    // 装备ID为空时不能修改
    if (EquipmentId.IsNone())
    {
        return false;
    }

    // 记录本次装备状态是否真正发生变化
    bool bStateChanged = false;

    // 处理启用装备
    if (bActive)
    {
        // 当前尚未启用该装备时加入数组
        if (!ActiveCourseEquipmentIds.Contains(EquipmentId))
        {
            ActiveCourseEquipmentIds.Add(EquipmentId);
            bStateChanged = true;
        }
    }
    else
    {
        // 从已启用装备数组中删除指定装备
        bStateChanged = ActiveCourseEquipmentIds.Remove(EquipmentId) > 0;
    }

    // 状态与原来一致时不需要重复强制网络更新
    if (!bStateChanged)
    {
        UE_LOG(LogTemp, Warning, TEXT("课程装备状态没有变化：学员SN=%s，装备=%s，启用=%s"), IsValid(OwnerPlayer) ? *OwnerPlayer->GPlayerInfo.SN : TEXT("无"), *EquipmentId.ToString(), bActive ? TEXT("是") : TEXT("否"));
        return true;
    }

    // 获取任务组件所属玩家Actor
    AActor* ComponentOwner = GetOwner();

    // 强制所属玩家尽快复制最新装备状态
    if (IsValid(ComponentOwner))
    {
        ComponentOwner->ForceNetUpdate();
    }

    // 输出服务器最终保存的课程装备状态
    //UE_LOG(LogTemp, Warning, TEXT("课程装备状态修改完成：学员SN=%s，装备=%s，启用=%s，当前启用装备数量=%d"), IsValid(OwnerPlayer) ? *OwnerPlayer->GPlayerInfo.SN : TEXT("无"), *EquipmentId.ToString(), bActive ? TEXT("是") : TEXT("否"), ActiveCourseEquipmentIds.Num());

    // 返回状态修改成功
    return true;
}

// 记录VR手进入的课程实体目标
void UAC_Task::EnterCourseVRTarget(UPrimitiveComponent* HandComponent, FName TargetId)
{
    // 只有本地VR学员端记录触碰目标
    if (!IsValid(OwnerPlayer) || OwnerPlayer->ProcessType != EProcessType::VR_XYD || !OwnerPlayer->IsLocallyControlled())
    {
        return;
    }

    // 判断手部碰撞组件和目标ID是否有效
    if (!IsValid(HandComponent) || TargetId.IsNone())
    {
        return;
    }

    // 保存这只手当前触碰的目标
    TouchedCourseVRTargets.Add(HandComponent, TargetId);

    // 输出触碰日志
    UE_LOG(LogTemp, Warning, TEXT("VR手进入课程目标：目标=%s"), *TargetId.ToString());
}

// 清除VR手离开的课程实体目标
void UAC_Task::LeaveCourseVRTarget(UPrimitiveComponent* HandComponent, FName TargetId)
{
    // 判断手部碰撞组件是否有效
    if (!IsValid(HandComponent))
    {
        return;
    }

    // 查找这只手当前记录的目标
    const FName* CurrentTargetId = TouchedCourseVRTargets.Find(HandComponent);

    // 只有离开的目标和当前记录一致时才清除
    if (CurrentTargetId && *CurrentTargetId == TargetId)
    {
        // 清除这只手的触碰记录
        TouchedCourseVRTargets.Remove(HandComponent);

        // 输出离开日志
        UE_LOG(LogTemp, Warning, TEXT("VR手离开课程目标：目标=%s"), *TargetId.ToString());
    }
}

// 提交指定VR手当前触碰的课程目标
bool UAC_Task::TrySubmitTouchedCourseVRTarget(UPrimitiveComponent* HandComponent)
{
    // 只有本地VR学员端允许提交实体目标
    if (!IsValid(OwnerPlayer) || OwnerPlayer->ProcessType != EProcessType::VR_XYD || !OwnerPlayer->IsLocallyControlled())
    {
        return false;
    }

    // 判断手部碰撞组件是否有效
    if (!IsValid(HandComponent))
    {
        return false;
    }

    // 查找这只手当前触碰的目标
    const FName* TouchedTargetId = TouchedCourseVRTargets.Find(HandComponent);

    // 没有触碰课程目标时不能提交
    if (!TouchedTargetId || TouchedTargetId->IsNone())
    {
        return false;
    }

    // 使用当前任务的TargetId验证触碰目标
    const bool bSubmitSucceeded = TrySubmitCurrentCourseTarget(*TouchedTargetId);

    // 提交成功后清除记录，防止连续扣动扳机重复提交
    if (bSubmitSucceeded)
    {
        TouchedCourseVRTargets.Remove(HandComponent);
    }

    // 返回本次提交结果
    return bSubmitSucceeded;
}

// 统一处理VR玩家扣动扳机时当前触碰的课程目标
void UAC_Task::HandleCourseVRTargetTrigger(UPrimitiveComponent* HandComponent)
{
    // 暂停期间不执行VR目标自身功能，也不提交课程事件
    if (bTrainPaused)
    {
        UE_LOG(LogTemp, Warning, TEXT("VR课程目标触发已忽略：训练处于暂停状态"));
        return;
    }

    // 传入的VR手部组件无效时不能继续处理
    if (!IsValid(HandComponent))
    {
        return;
    }

    // 查找当前VR手正在触碰的课程目标
    const FName* TouchedTargetId = TouchedCourseVRTargets.Find(HandComponent);

    // 当前手没有触碰有效课程目标时不执行
    if (!TouchedTargetId || TouchedTargetId->IsNone())
    {
        return;
    }

    // 保存本次实际触发的课程目标ID
    const FName TriggeredTargetId = *TouchedTargetId;

    // 先通知蓝图执行装备或设备本身的功能
    OnCourseVRTargetTriggered_BIE(TriggeredTargetId, HandComponent);

    // 当前任务开放了装备选择权限时按照课程装备处理
    if (CanOpenCourseEquipmentSelectionUI())
    {
        // 保存装备状态，并根据当前任务类型决定是否尝试完成任务
        const bool bEquipmentSelectionAccepted = SubmitCourseEquipmentSelection(TriggeredTargetId);

        // 本次装备选择已经完成处理时清除当前手的触碰记录
        if (bEquipmentSelectionAccepted)
        {
            TouchedCourseVRTargets.Remove(HandComponent);
        }

        // 输出VR装备处理结果
        UE_LOG(LogTemp, Warning, TEXT("VR课程装备触发完成：任务=%s，装备=%s，选择被接受=%s"), IsValid(CurrentTask) ? *CurrentTask->TaskData.TaskName : TEXT("无"), *TriggeredTargetId.ToString(), bEquipmentSelectionAccepted ? TEXT("是") : TEXT("否"));

        // 装备选择已经处理完成，不再重复执行普通课程目标提交
        return;
    }

    // 获取当前正在执行的数据驱动任务
    UDataDrivenTaskItem* CurrentDataDrivenTask = Cast<UDataDrivenTaskItem>(CurrentTask);

    // 当前不是数据驱动任务时只执行目标自身功能
    if (!IsValid(CurrentDataDrivenTask))
    {
        UE_LOG(LogTemp, Warning, TEXT("VR课程目标只执行自身功能：当前不是数据驱动任务，目标=%s"), *TriggeredTargetId.ToString());
        return;
    }

    // 获取当前任务或当前顺序阶段正在等待的完成条件
    FName CurrentExpectedEventId = NAME_None;
    FName CurrentExpectedTargetId = NAME_None;
    const bool bCurrentTaskAcceptsTargetSubmission = CurrentDataDrivenTask->GetCurrentCourseEventCondition(CurrentExpectedEventId, CurrentExpectedTargetId) && !CurrentExpectedTargetId.IsNone();

    // 没有配置实体目标完成条件时只执行目标自身功能
    if (!bCurrentTaskAcceptsTargetSubmission)
    {
        UE_LOG(LogTemp, Warning, TEXT("VR课程目标只执行自身功能但不提交任务：任务=%s，触发类型=%s，目标=%s"), *CurrentDataDrivenTask->Definition.TaskData.TaskName, *UEnum::GetValueAsString(CurrentDataDrivenTask->Definition.TriggerType), *TriggeredTargetId.ToString());
        return;
    }

    // 使用当前任务或当前阶段的目标条件尝试提交
    TrySubmitTouchedCourseVRTarget(HandComponent);
}

void UAC_Task::ReportCourseSpeechText(int32 SpeechTaskIndex, const FString& RecognizedText)
{
    // 客户端暂停期间不再上传语音识别文本
    if (bTrainPaused)
    {
        UE_LOG(LogTemp, Warning, TEXT("课程语音文本上报已忽略：训练处于暂停状态"));
        return;
    }

    // 限制单次语音文本长度
    FString SafeRecognizedText = RecognizedText.Left(500);

    // 清理文本首尾空白
    SafeRecognizedText.TrimStartAndEndInline();

    // 空文本不发送给服务器
    if (SafeRecognizedText.IsEmpty())
    {
        return;
    }

    // 获取任务组件所属Actor
    AActor* ComponentOwner = GetOwner();

    // Owner无效时不能上报
    if (!IsValid(ComponentOwner))
    {
        UE_LOG(LogTemp, Error, TEXT("课程语音文本上报失败：任务组件Owner无效"));
        return;
    }

    // 服务器或单机直接处理
    if (ComponentOwner->HasAuthority())
    {
        HandleCourseSpeechText(SpeechTaskIndex, SafeRecognizedText);
        return;
    }

    // 客户端将语音文本发送给服务器
    RPC_ServerReportCourseSpeechText(SpeechTaskIndex, SafeRecognizedText);
}

void UAC_Task::SetCourseSpeechRecognitionAllowed(bool bAllowed)
{
    // 权限没有变化时不重复通知
    if (bCourseSpeechRecognitionAllowed == bAllowed)
    {
        return;
    }

    // 保存最新语音识别权限
    bCourseSpeechRecognitionAllowed = bAllowed;

    // 通知本地蓝图开始准备或停止录音
    OnCourseSpeechRecognitionPermissionChanged.Broadcast(bCourseSpeechRecognitionAllowed);

    // 输出权限变化日志
    //UE_LOG(LogTemp, Warning, TEXT("课程语音识别权限变化：允许识别=%s"), bCourseSpeechRecognitionAllowed ? TEXT("是") : TEXT("否"));
}

bool UAC_Task::CanUseCourseSpeechRecognition() const
{
    // 训练暂停期间不允许继续进行课程语音识别
    if (bTrainPaused)
    {
        return false;
    }

    // 当前任务没有开放语音识别
    if (!bCourseSpeechRecognitionAllowed)
    {
        return false;
    }

    // 玩家对象无效时不能进行识别
    if (!IsValid(OwnerPlayer))
    {
        return false;
    }

    // 只有PC学员和VR学员属于语音任务操作端
    if (OwnerPlayer->ProcessType != EProcessType::PC_XYD && OwnerPlayer->ProcessType != EProcessType::VR_XYD)
    {
        return false;
    }

    // 只有所属玩家的本地客户端允许操作
    return OwnerPlayer->IsLocallyControlled();
}

void UAC_Task::SetActiveCoursePCInputId(FName InputId)
{
    // 保存当前任务输入ID
    ActiveCoursePCInputId = InputId;

    // 输出输入权限变化日志
   // UE_LOG(LogTemp, Warning, TEXT("课程PC输入权限变化：输入=%s"), *ActiveCoursePCInputId.ToString());

}

bool UAC_Task::TryTriggerCoursePCInput(FName InputId)
{
    // 只允许本地PC学员调用
    if (!IsValid(OwnerPlayer) || OwnerPlayer->ProcessType != EProcessType::PC_XYD || !OwnerPlayer->IsLocallyControlled())
    {
        return false;
    }

    // 当前任务没有开放PC快捷输入时不处理
    if (ActiveCoursePCInputId.IsNone())
    {
        return false;
    }

    // 本次输入与当前任务配置不一致时不处理
    if (InputId.IsNone() || InputId != ActiveCoursePCInputId)
    {
        return false;
    }

    // 获取当前数据驱动任务
    UDataDrivenTaskItem* CurrentDataDrivenTask = Cast<UDataDrivenTaskItem>(CurrentTask);

    // 当前任务类型无效时不处理
    if (!IsValid(CurrentDataDrivenTask))
    {
        return false;
    }

    // 使用当前阶段或任务正在等待的目标ID执行通用目标提交
    return TrySubmitCurrentCourseTarget(GetCurrentCourseTargetId());

}

// 根据配置生成这个学员对应的本地场景Actor
void UAC_Task::SpawnCourseLocalActors(const TArray<FCourseLocalActorSpawnDefinition>& SpawnDefinitions)
{
    // 当前进程没有生成权限时直接返回
    if (!ShouldCreateCourseLocalActors())
    {
        return;
    }

    // 没有配置生成内容时不执行
    if (SpawnDefinitions.IsEmpty())
    {
        return;
    }

    // 获取当前World
    UWorld* CurrentWorld = GetWorld();

    // World无效时无法生成
    if (!IsValid(CurrentWorld))
    {
        return;
    }

    // 遍历任务配置的本地Actor
    for (const FCourseLocalActorSpawnDefinition& SpawnDefinition : SpawnDefinitions)
    {
        // 检查本地Actor ID
        if (SpawnDefinition.ActorId.IsNone())
        {
            UE_LOG(LogTemp, Error, TEXT("课程本地Actor生成失败：ActorId为空"));
            continue;
        }

        // 检查需要生成的Actor类
        if (!SpawnDefinition.ActorClass)
        {
            UE_LOG(LogTemp, Error, TEXT("课程本地Actor生成失败：ActorClass为空，ActorId=%s"), *SpawnDefinition.ActorId.ToString());
            continue;
        }

        // 检查生成锚点ID
        if (SpawnDefinition.SpawnAnchorId.IsNone())
        {
            UE_LOG(LogTemp, Error, TEXT("课程本地Actor生成失败：SpawnAnchorId为空，ActorId=%s"), *SpawnDefinition.ActorId.ToString());
            continue;
        }

        // 查找是否已经生成过相同ID的Actor
        if (TObjectPtr<AActor>* ExistingActor = SpawnedCourseLocalActors.Find(SpawnDefinition.ActorId))
        {
            // Actor仍然有效时避免重复生成
            if (IsValid(ExistingActor->Get()))
            {
                continue;
            }

            // 清除已经失效的记录
            SpawnedCourseLocalActors.Remove(SpawnDefinition.ActorId);
        }

        // 保存找到的生成锚点
        TArray<AActor*> FoundAnchorActors;

        // 根据配置的Tag查找场景锚点
        UGameplayStatics::GetAllActorsWithTag(CurrentWorld, SpawnDefinition.SpawnAnchorId, FoundAnchorActors);

        // 没有找到锚点时不能生成
        if (FoundAnchorActors.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("课程本地Actor生成失败：没有找到锚点，ActorId=%s，锚点=%s"), *SpawnDefinition.ActorId.ToString(), *SpawnDefinition.SpawnAnchorId.ToString());
            continue;
        }

        // 获取第一个有效锚点
        AActor* SpawnAnchorActor = FoundAnchorActors[0];

        // 检查锚点是否有效
        if (!IsValid(SpawnAnchorActor))
        {
            continue;
        }

        // 保存生成位置和旋转
        const FTransform SpawnTransform = SpawnAnchorActor->GetActorTransform();

        // 延迟生成Actor，以便在BeginPlay前关闭网络复制
        AActor* SpawnedActor = CurrentWorld->SpawnActorDeferred<AActor>(SpawnDefinition.ActorClass, SpawnTransform, OwnerPlayer, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

        // 检查Actor是否生成成功
        if (!IsValid(SpawnedActor))
        {
            UE_LOG(LogTemp, Error, TEXT("课程本地Actor生成失败：SpawnActorDeferred失败，ActorId=%s"), *SpawnDefinition.ActorId.ToString());
            continue;
        }

        // 强制设置为本地Actor，不参与服务器复制
        SpawnedActor->SetReplicates(false);

        // 添加通用本地角色标签
        SpawnedActor->Tags.AddUnique(FName(TEXT("课程本地角色")));

        // 添加配置的Actor ID标签
        SpawnedActor->Tags.AddUnique(SpawnDefinition.ActorId);

        // 使用当前任务所属玩家作为Owner，像素流端可以区分角色属于哪个学员
        SpawnedActor->SetOwner(OwnerPlayer);

        // 完成Actor生成并执行其BeginPlay
        UGameplayStatics::FinishSpawningActor(SpawnedActor, SpawnTransform);

        // 保存生成结果
        SpawnedCourseLocalActors.Add(SpawnDefinition.ActorId, SpawnedActor);

        // 输出生成日志
       // UE_LOG(LogTemp, Warning, TEXT("课程本地Actor生成成功：学员=%s，ActorId=%s，Actor=%s"), *OwnerPlayer->GPlayerInfo.SN, *SpawnDefinition.ActorId.ToString(), *SpawnedActor->GetName());
    }
}

// 设置当前任务允许使用的VR输入
void UAC_Task::SetActiveCourseVRInputId(FName InputId)
{
    // 保存当前任务输入ID
    ActiveCourseVRInputId = InputId;

    // 输出VR输入权限变化日志
    //UE_LOG(LogTemp, Warning, TEXT("课程VR输入权限变化：输入=%s"), *ActiveCourseVRInputId.ToString());
}

bool UAC_Task::TryTriggerCourseVRInput(FName InputId)
{
    // 只允许本地VR学员调用
    if (!IsValid(OwnerPlayer) || OwnerPlayer->ProcessType != EProcessType::VR_XYD || !OwnerPlayer->IsLocallyControlled())
    {
        return false;
    }

    // 当前任务没有开放VR输入时不处理
    if (ActiveCourseVRInputId.IsNone())
    {
        return false;
    }

    // 本次输入与当前任务配置不一致时不处理
    if (InputId.IsNone() || InputId != ActiveCourseVRInputId)
    {
        return false;
    }

    // 获取当前数据驱动任务
    UDataDrivenTaskItem* CurrentDataDrivenTask = Cast<UDataDrivenTaskItem>(CurrentTask);

    // 当前任务类型无效时不处理
    if (!IsValid(CurrentDataDrivenTask))
    {
        return false;
    }

    // 使用当前阶段或任务正在等待的目标ID执行通用目标提交
    return TrySubmitCurrentCourseTarget(GetCurrentCourseTargetId());
}

void UAC_Task::RemoveCourseLocalActor(FName ActorId)
{
    // ActorId无效时不处理
    if (ActorId.IsNone())
    {
        return;
    }

    // 查找当前任务组件保存的本地Actor
    TObjectPtr<AActor>* FoundActor = SpawnedCourseLocalActors.Find(ActorId);

    // 没有找到对应Actor时直接返回
    if (!FoundActor)
    {
        return;
    }

    // 销毁仍然有效的Actor
    if (IsValid(FoundActor->Get()))
    {
        FoundActor->Get()->Destroy();
    }

    // 清除本地Actor记录
    SpawnedCourseLocalActors.Remove(ActorId);

    // 输出清理日志
    //UE_LOG(LogTemp, Warning, TEXT("课程本地Actor清理完成：ActorId=%s"), *ActorId.ToString());
}

// 执行任务配置的全部本地Actor动作
void UAC_Task::ExecuteCourseLocalActorActions(const TArray<FCourseLocalActorActionDefinition>& ActionDefinitions)
{
    // 当前进程没有本地Actor表现权限时直接返回
    if (!ShouldCreateCourseLocalActors())
    {
        return;
    }

    // 没有动作配置时直接返回
    if (ActionDefinitions.IsEmpty())
    {
        return;
    }

    // 获取当前World
    UWorld* CurrentWorld = GetWorld();

    // World无效时无法设置延时动作
    if (!IsValid(CurrentWorld))
    {
        return;
    }

    // 遍历任务配置的全部本地Actor动作
    for (const FCourseLocalActorActionDefinition& ActionDefinition : ActionDefinitions)
    {
        // 没有延时时立即执行动作
        if (ActionDefinition.StartDelay <= KINDA_SMALL_NUMBER)
        {
            ExecuteSingleCourseLocalActorAction(ActionDefinition);
            continue;
        }

        // 创建本次动作的延时计时器
        FTimerHandle DelayTimerHandle;

        // 创建不会在任务组件销毁后继续调用的弱引用委托
        FTimerDelegate DelayTimerDelegate = FTimerDelegate::CreateWeakLambda(this, [this, ActionDefinition]() { ExecuteSingleCourseLocalActorAction(ActionDefinition); });

        // 根据动作配置设置一次性延时计时器
        CurrentWorld->GetTimerManager().SetTimer(DelayTimerHandle, DelayTimerDelegate, ActionDefinition.StartDelay, false);

        // 保存计时器以便任务结束时清理
        CourseLocalActorActionDelayTimerHandles.Add(DelayTimerHandle);

        // 输出延时动作日志
        UE_LOG(LogTemp, Warning, TEXT("课程本地Actor动作已加入延时队列：ActorId=%s，ActionId=%s，延时=%.2f秒"), *ActionDefinition.ActorId.ToString(), *ActionDefinition.ActionId.ToString(), ActionDefinition.StartDelay);
    }
}

// 执行任务配置的全部本地Actor通用命令
void UAC_Task::ExecuteCourseLocalActorCommands(const TArray<FCourseLocalActorCommandDefinition>& CommandDefinitions)
{
    // 当前进程没有本地Actor表现权限时不执行命令
    if (!ShouldCreateCourseLocalActors())
    {
        return;
    }

    // 没有配置命令时直接返回
    if (CommandDefinitions.IsEmpty())
    {
        return;
    }

    // 遍历当前任务配置的全部本地Actor命令
    for (const FCourseLocalActorCommandDefinition& CommandDefinition : CommandDefinitions)
    {
        // 立即执行当前本地Actor命令
        ExecuteSingleCourseLocalActorCommand(CommandDefinition);
    }
}

void UAC_Task::ClearCourseLocalActorActionDelayTimers()
{
    // 获取当前World
    UWorld* CurrentWorld = GetWorld();

    // World有效时清理全部计时器
    if (IsValid(CurrentWorld))
    {
        // 遍历当前任务创建的全部延时动作计时器
        for (FTimerHandle& DelayTimerHandle : CourseLocalActorActionDelayTimerHandles)
        {
            // 清除当前延时动作计时器
            CurrentWorld->GetTimerManager().ClearTimer(DelayTimerHandle);
        }
    }

    // 清空计时器记录
    CourseLocalActorActionDelayTimerHandles.Empty();
}

void UAC_Task::RPC_ServerSetCourseIntegerSelection_Implementation(int32 IntegerValue)
{
    // 服务器拒绝暂停期间到达的整数选择
    if (bTrainPaused)
    {
        UE_LOG(LogTemp, Warning, TEXT("服务器拒绝暂停期间的课程整数选择：整数=%d"), IntegerValue);
        return;
    }

    // 再次验证客户端上传的选择范围
    if (IntegerValue < 0 || IntegerValue > 3)
    {
        // 拒绝保存非法选择
        UE_LOG(LogTemp, Error, TEXT("服务器拒绝课程整数选择：整数超出范围，整数=%d"), IntegerValue);
        return;
    }

    // 保存服务器权威选择值
    CurrentIntegerSelection = IntegerValue;

    // 通知服务器本地UI刷新
    OnCourseIntegerSelectionChanged.Broadcast(CurrentIntegerSelection);

    // 获取任务组件所属的玩家Actor
    AActor* ComponentOwner = GetOwner();

    // 判断所属Actor是否有效
    if (IsValid(ComponentOwner))
    {
        // 让选择结果尽快复制到像素流客户端
        ComponentOwner->ForceNetUpdate();
    }

    // 输出服务器保存结果
    UE_LOG(LogTemp, Warning, TEXT("服务器保存课程整数选择：整数=%d"), CurrentIntegerSelection);
}

void UAC_Task::OnRep_CurrentIntegerSelection()
{
    // 输出日志，用于确认像素流端已经收到选择
    UE_LOG(LogTemp, Warning, TEXT("客户端收到课程整数选择：整数=%d"), CurrentIntegerSelection);

    // 通知当前客户端自己的UI刷新CheckBox
    OnCourseIntegerSelectionChanged.Broadcast(CurrentIntegerSelection);
}

// 判断当前进程是否应该生成这个学员的本地场景Actor
bool UAC_Task::ShouldCreateCourseLocalActors() const
{
    // 玩家或者World无效时不能生成
    if (!IsValid(OwnerPlayer) || !IsValid(GetWorld()))
    {
        return false;
    }

    // Dedicated Server绝对不生成表现Actor
    if (GetWorld()->GetNetMode() == NM_DedicatedServer)
    {
        return false;
    }

    // 只有PC或VR学员的任务才需要生成现场角色
    const bool bOwnerIsStudent = OwnerPlayer->ProcessType == EProcessType::PC_XYD || OwnerPlayer->ProcessType == EProcessType::VR_XYD;

    // 排除教练、像素流观察Pawn以及其他非学员任务
    if (!bOwnerIsStudent)
    {
        return false;
    }

    // 当前任务所属学员自己的本地客户端允许生成
    if (OwnerPlayer->IsLocallyControlled())
    {
        return true;
    }

    // 获取当前客户端自己的进程类型
    UGameInstance* CurrentGameInstance = GetWorld()->GetGameInstance();

    // GameInstance无效时不能继续判断
    if (!IsValid(CurrentGameInstance))
    {
        return false;
    }

    // 获取当前客户端的信息子系统
    UInfoSubsystem* InfoSubsystem = CurrentGameInstance->GetSubsystem<UInfoSubsystem>();

    // 远端像素流客户端为每个学员生成对应的本地场景Actor
    return IsValid(InfoSubsystem) && InfoSubsystem->ProcessType == EProcessType::ViewPort;
}

// 清理这个学员已经生成的全部本地场景Actor
void UAC_Task::ClearCourseLocalActors()
{
    // 遍历全部本地Actor记录
    for (TPair<FName, TObjectPtr<AActor>>& ActorPair : SpawnedCourseLocalActors)
    {
        // 销毁仍然有效的本地Actor
        if (IsValid(ActorPair.Value.Get()))
        {
            ActorPair.Value->Destroy();
        }
    }

    // 清空全部Actor引用
    SpawnedCourseLocalActors.Empty();
}

void UAC_Task::ExecuteSingleCourseLocalActorAction(const FCourseLocalActorActionDefinition& ActionDefinition)
{
    // 再次检查当前进程是否仍然拥有本地表现权限
    if (!ShouldCreateCourseLocalActors())
    {
        return;
    }

    // 检查本地Actor ID
    if (ActionDefinition.ActorId.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor动作执行失败：ActorId为空"));
        return;
    }

    // 需要播放动画时必须配置动作ID
    if (ActionDefinition.bPlayAnimation && ActionDefinition.ActionId.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor动作执行失败：需要播放动画但ActionId为空，ActorId=%s"), *ActionDefinition.ActorId.ToString());
        return;
    }

    // 纯声音动作必须配置ActionSound
    if (!ActionDefinition.bPlayAnimation && !IsValid(ActionDefinition.ActionSound.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor动作执行失败：纯声音动作没有配置ActionSound，ActorId=%s"), *ActionDefinition.ActorId.ToString());
        return;
    }

    // 根据Actor ID查找本地Actor
    TObjectPtr<AActor>* FoundActor = SpawnedCourseLocalActors.Find(ActionDefinition.ActorId);

    // 检查本地Actor是否存在
    if (!FoundActor || !IsValid(FoundActor->Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor动作执行失败：没有找到Actor，ActorId=%s"), *ActionDefinition.ActorId.ToString());
        return;
    }

    // 获取需要执行动作的Actor
    AActor* TargetActor = FoundActor->Get();

    // 检查Actor是否实现通用动作接口
    if (!TargetActor->GetClass()->ImplementsInterface(UCourseLocalActorActionInterface::StaticClass()))
    {
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor动作执行失败：Actor未实现动作接口，ActorId=%s"), *ActionDefinition.ActorId.ToString());
        return;
    }

    // 将完整动作配置传给目标Actor
    ICourseLocalActorActionInterface::Execute_PlayCourseLocalActorAction(TargetActor, ActionDefinition);

    // 当前动作启用了路线移动时，同时启动Actor上的通用路线移动组件
    ExecuteCourseLocalActorPathMove(TargetActor, ActionDefinition);

    // 输出动作开始日志
    UE_LOG(LogTemp, Warning, TEXT("课程本地Actor开始执行动作：ActorId=%s，ActionId=%s"), *ActionDefinition.ActorId.ToString(), *ActionDefinition.ActionId.ToString());

}

void UAC_Task::ExecuteCourseLocalActorPathMove(AActor* TargetActor, const FCourseLocalActorActionDefinition& ActionDefinition)
{
    // 当前动作没有启用路线移动时保持原有动作逻辑
    if (!ActionDefinition.PathMove.bEnabled)
    {
        return;
    }

    // 检查需要移动的Actor是否仍然有效
    if (!IsValid(TargetActor))
    {
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor路线移动启动失败：目标Actor无效，ActorId=%s"), *ActionDefinition.ActorId.ToString());
        return;
    }

    // 路线ID为空时无法确定需要使用哪条路线
    if (ActionDefinition.PathMove.RouteId.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor路线移动启动失败：路线ID为空，ActorId=%s"), *ActionDefinition.ActorId.ToString());
        return;
    }

    // 检查目标Actor是否添加了通用路线移动组件
    UCourseNPCPathMovementComponent* PathMovementComponent = TargetActor->FindComponentByClass<UCourseNPCPathMovementComponent>();

    if (!IsValid(PathMovementComponent))
    {
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor路线移动启动失败：Actor没有CourseNPCPathMovementComponent，ActorId=%s，Actor=%s"), *ActionDefinition.ActorId.ToString(), *TargetActor->GetName());
        return;
    }

    // 获取当前World，用于查找场景中的路线Actor
    UWorld* CurrentWorld = GetWorld();

    if (!IsValid(CurrentWorld))
    {
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor路线移动启动失败：World无效，ActorId=%s"), *ActionDefinition.ActorId.ToString());
        return;
    }

    // 查找场景中全部通用NPC路线Actor
    TArray<AActor*> FoundPathActors;
    UGameplayStatics::GetAllActorsOfClass(CurrentWorld, ACourseNPCPathActor::StaticClass(), FoundPathActors);

    // 保存第一个路线ID匹配的有效路线Actor
    ACourseNPCPathActor* TargetPathActor = nullptr;
    int32 MatchedPathCount = 0;

    // 根据配置的路线ID查找对应路线
    for (AActor* FoundPathActor : FoundPathActors)
    {
        ACourseNPCPathActor* CourseNPCPathActor = Cast<ACourseNPCPathActor>(FoundPathActor);

        // 忽略已经失效或类型不正确的Actor
        if (!IsValid(CourseNPCPathActor))
        {
            continue;
        }

        // 忽略路线ID不匹配的路线
        if (CourseNPCPathActor->RouteId != ActionDefinition.PathMove.RouteId)
        {
            continue;
        }

        // 记录匹配的路线数量
        MatchedPathCount++;

        // 保存第一个匹配的路线Actor
        if (!IsValid(TargetPathActor))
        {
            TargetPathActor = CourseNPCPathActor;
        }
    }

    // 没有找到匹配路线时不能启动移动
    if (!IsValid(TargetPathActor))
    {
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor路线移动启动失败：没有找到路线，ActorId=%s，路线ID=%s"), *ActionDefinition.ActorId.ToString(), *ActionDefinition.PathMove.RouteId.ToString());
        return;
    }

    // 场景中出现重复路线ID时输出警告并继续使用第一个
    if (MatchedPathCount > 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("课程本地Actor路线移动找到多个相同路线ID：路线ID=%s，数量=%d，将使用第一个"), *ActionDefinition.PathMove.RouteId.ToString(), MatchedPathCount);
    }

    // 保存路线移动配置，避免后续代码重复访问嵌套结构
    const FCourseLocalActorPathMoveDefinition& PathMoveDefinition = ActionDefinition.PathMove;

    // 使用配置中的速度、方向和朝向设置启动路线移动
    const bool bStartSucceeded = PathMovementComponent->StartCourseNPCPathMove(TargetPathActor, PathMoveDefinition.MoveSpeed, PathMoveDefinition.bMoveReverse, PathMoveDefinition.bFaceMovementDirection, ActionDefinition.ActorId, PathMoveDefinition.ArrivalIdleActionId);

    // 检查通用路线移动组件是否启动成功
    if (!bStartSucceeded)
    {
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor路线移动启动失败：移动组件拒绝启动，ActorId=%s，路线ID=%s"), *ActionDefinition.ActorId.ToString(), *PathMoveDefinition.RouteId.ToString());
        return;
    }

    // 输出配置驱动路线移动成功日志
    UE_LOG(LogTemp, Warning, TEXT("课程本地Actor路线移动启动成功：ActorId=%s，路线ID=%s，速度=%.2f，反向=%s"), *ActionDefinition.ActorId.ToString(), *PathMoveDefinition.RouteId.ToString(), PathMoveDefinition.MoveSpeed, PathMoveDefinition.bMoveReverse ? TEXT("是") : TEXT("否"));
}

void UAC_Task::ExecuteSingleCourseLocalActorCommand(const FCourseLocalActorCommandDefinition& CommandDefinition)
{
    // 当前进程没有本地Actor表现权限时不执行命令
    if (!ShouldCreateCourseLocalActors())
    {
        return;
    }

    // ActorId为空时无法确定需要操作的Actor
    if (CommandDefinition.ActorId.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor命令执行失败：ActorId为空"));
        return;
    }

    // 根据ActorId查找当前客户端已经生成的本地Actor
    TObjectPtr<AActor>* FoundActor = SpawnedCourseLocalActors.Find(CommandDefinition.ActorId);

    // 没有找到Actor或者Actor已经失效时不能执行命令
    if (!FoundActor || !IsValid(FoundActor->Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor命令执行失败：没有找到有效Actor，ActorId=%s"), *CommandDefinition.ActorId.ToString());
        return;
    }

    // 获取当前需要执行命令的Actor
    AActor* TargetActor = FoundActor->Get();

    // 根据命令类型执行对应的通用处理
    switch (CommandDefinition.CommandType)
    {
    case ECourseLocalActorCommandType::None:
    {
        // 没有配置命令类型时不执行
        UE_LOG(LogTemp, Warning, TEXT("课程本地Actor命令已忽略：CommandType为None，ActorId=%s"), *CommandDefinition.ActorId.ToString());
        return;
    }

    case ECourseLocalActorCommandType::DestroyActor:
    {
        // 使用任务组件现有删除入口销毁Actor并清除Map记录
        RemoveCourseLocalActor(CommandDefinition.ActorId);

        // 输出销毁命令完成日志
        UE_LOG(LogTemp, Warning, TEXT("课程本地Actor销毁命令执行完成：ActorId=%s"), *CommandDefinition.ActorId.ToString());
        return;
    }

    case ECourseLocalActorCommandType::SetTransformFromAnchor:
    {
        // 没有启用任何变换项目时不执行
        if (!CommandDefinition.bApplyLocation && !CommandDefinition.bApplyRotation && !CommandDefinition.bApplyScale)
        {
            UE_LOG(LogTemp, Warning, TEXT("课程本地Actor变换命令已忽略：没有启用位置、旋转或缩放，ActorId=%s"), *CommandDefinition.ActorId.ToString());
            return;
        }

        // 检查目标锚点ID
        if (CommandDefinition.TargetAnchorId.IsNone())
        {
            UE_LOG(LogTemp, Error, TEXT("课程本地Actor变换命令执行失败：TargetAnchorId为空，ActorId=%s"), *CommandDefinition.ActorId.ToString());
            return;
        }

        // 获取当前World
        UWorld* CurrentWorld = GetWorld();

        // World无效时无法查找场景锚点
        if (!IsValid(CurrentWorld))
        {
            UE_LOG(LogTemp, Error, TEXT("课程本地Actor变换命令执行失败：World无效，ActorId=%s"), *CommandDefinition.ActorId.ToString());
            return;
        }

        // 保存拥有指定Tag的全部场景Actor
        TArray<AActor*> FoundAnchorActors;

        // 根据TargetAnchorId查找场景锚点
        UGameplayStatics::GetAllActorsWithTag(CurrentWorld, CommandDefinition.TargetAnchorId, FoundAnchorActors);

        // 没有找到锚点时不能设置Actor变换
        if (FoundAnchorActors.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("课程本地Actor变换命令执行失败：没有找到锚点，ActorId=%s，锚点=%s"), *CommandDefinition.ActorId.ToString(), *CommandDefinition.TargetAnchorId.ToString());
            return;
        }

        // 出现多个同名锚点时输出警告并继续使用第一个有效锚点
        if (FoundAnchorActors.Num() > 1)
        {
            UE_LOG(LogTemp, Warning, TEXT("课程本地Actor变换命令找到多个同名锚点：锚点=%s，数量=%d，将使用第一个有效锚点"), *CommandDefinition.TargetAnchorId.ToString(), FoundAnchorActors.Num());
        }

        // 保存最终找到的有效锚点
        AActor* TargetAnchorActor = nullptr;

        // 从查找结果中取得第一个有效锚点
        for (AActor* FoundAnchorActor : FoundAnchorActors)
        {
            // 忽略已经失效的锚点Actor
            if (!IsValid(FoundAnchorActor))
            {
                continue;
            }

            // 保存第一个有效锚点
            TargetAnchorActor = FoundAnchorActor;

            // 已经找到有效锚点，不再继续遍历
            break;
        }

        // 没有取得有效锚点时不能继续
        if (!IsValid(TargetAnchorActor))
        {
            UE_LOG(LogTemp, Error, TEXT("课程本地Actor变换命令执行失败：找到的锚点全部无效，ActorId=%s，锚点=%s"), *CommandDefinition.ActorId.ToString(), *CommandDefinition.TargetAnchorId.ToString());
            return;
        }

        // 根据相对变换和锚点世界变换计算最终目标变换
        const FTransform TargetWorldTransform = CommandDefinition.RelativeTransform * TargetAnchorActor->GetActorTransform();

        // 默认保留Actor当前没有要求修改的变换数据
        FTransform FinalActorTransform = TargetActor->GetActorTransform();

        // 根据配置设置Actor位置
        if (CommandDefinition.bApplyLocation)
        {
            FinalActorTransform.SetLocation(TargetWorldTransform.GetLocation());
        }

        // 根据配置设置Actor旋转
        if (CommandDefinition.bApplyRotation)
        {
            FinalActorTransform.SetRotation(TargetWorldTransform.GetRotation());
        }

        // 根据配置设置Actor缩放
        if (CommandDefinition.bApplyScale)
        {
            FinalActorTransform.SetScale3D(TargetWorldTransform.GetScale3D());
        }

        // 使用瞬移方式设置最终变换，避免角色物理状态产生错误插值
        const bool bTransformSucceeded = TargetActor->SetActorTransform(FinalActorTransform, false, nullptr, ETeleportType::TeleportPhysics);

        // 检查Actor变换是否设置成功
        if (!bTransformSucceeded)
        {
            UE_LOG(LogTemp, Error, TEXT("课程本地Actor变换命令执行失败：SetActorTransform失败，ActorId=%s"), *CommandDefinition.ActorId.ToString());
            return;
        }

        // 输出Actor变换成功日志
        UE_LOG(LogTemp, Warning, TEXT("课程本地Actor变换命令执行完成：ActorId=%s，锚点=%s，设置位置=%s，设置旋转=%s，设置缩放=%s"), *CommandDefinition.ActorId.ToString(), *CommandDefinition.TargetAnchorId.ToString(), CommandDefinition.bApplyLocation ? TEXT("是") : TEXT("否"), CommandDefinition.bApplyRotation ? TEXT("是") : TEXT("否"), CommandDefinition.bApplyScale ? TEXT("是") : TEXT("否"));
        return;
    }

    case ECourseLocalActorCommandType::Custom:
    {
        // 自定义命令ID为空时无法确定Actor需要执行的功能
        if (CommandDefinition.CustomCommandId.IsNone())
        {
            UE_LOG(LogTemp, Error, TEXT("课程本地Actor自定义命令执行失败：CustomCommandId为空，ActorId=%s"), *CommandDefinition.ActorId.ToString());
            return;
        }

        // 检查目标Actor是否实现自定义命令接口
        if (!TargetActor->GetClass()->ImplementsInterface(UCourseLocalActorCommandInterface::StaticClass()))
        {
            UE_LOG(LogTemp, Error, TEXT("课程本地Actor自定义命令执行失败：Actor没有实现命令接口，ActorId=%s，命令=%s"), *CommandDefinition.ActorId.ToString(), *CommandDefinition.CustomCommandId.ToString());
            return;
        }

        // 将完整命令配置发送给目标Actor
        ICourseLocalActorCommandInterface::Execute_ExecuteCourseLocalActorCustomCommand(TargetActor, CommandDefinition);

        // 输出自定义命令发送成功日志
        UE_LOG(LogTemp, Warning, TEXT("课程本地Actor自定义命令发送完成：ActorId=%s，命令=%s"), *CommandDefinition.ActorId.ToString(), *CommandDefinition.CustomCommandId.ToString());
        return;
    }

    default:
    {
        // 输出没有处理的枚举类型
        UE_LOG(LogTemp, Error, TEXT("课程本地Actor命令执行失败：存在未处理的CommandType，ActorId=%s"), *CommandDefinition.ActorId.ToString());
        return;
    }
    }
}

// 在服务器上处理完整的课程事件数据
void UAC_Task::HandleCourseEvent(const FCourseEventData& EventData)
{
    // 服务器最终拒绝暂停期间到达的课程事件
    if (bTrainPaused)
    {
        UE_LOG(LogTemp, Warning, TEXT("服务器拒绝暂停期间的课程事件：事件=%s，目标=%s"), *EventData.EventId.ToString(), *EventData.TargetId.ToString());
        return;
    }

    // 判断当前任务索引是否存在于运行时任务数组中
    if (!RuntimeTaskInstances.IsValidIndex(CurrentTaskIndex))
    {
        // 输出错误日志，说明服务器找不到当前任务
        UE_LOG(LogTemp, Error, TEXT("课程事件处理失败：当前任务索引无效，索引=%d"), CurrentTaskIndex);
        return;
    }

    // 将当前运行时任务转换成数据驱动任务
    UDataDrivenTaskItem* DataDrivenTaskInstance = Cast<UDataDrivenTaskItem>(RuntimeTaskInstances[CurrentTaskIndex]);

    // 判断当前任务是否为数据驱动任务
    if (!IsValid(DataDrivenTaskInstance))
    {
        // 非数据驱动任务不处理当前课程事件
        return;
    }

    // 输出服务器实际收到的课程事件数据
    //UE_LOG(LogTemp, Warning, TEXT("服务器收到课程事件：事件=%s，目标=%s，携带整数=%s，整数=%d"), *EventData.EventId.ToString(), *EventData.TargetId.ToString(), EventData.bHasIntegerValue ? TEXT("是") : TEXT("否"), EventData.IntegerValue);

        // 顺序阶段任务使用事件推进当前阶段
    if (DataDrivenTaskInstance->Definition.TriggerType == ECourseTaskTriggerType::StageSequence)
    {
        // 保存事件匹配成功后需要进入的下一阶段索引
        int32 NextStageIndex = INDEX_NONE;

        // 将事件交给当前顺序阶段任务校验
        const bool bStageEventMatched = DataDrivenTaskInstance->TryAdvanceByStageEvent(EventData, NextStageIndex);

        // 事件不匹配当前阶段时保持当前阶段继续等待
        if (!bStageEventMatched)
        {
            return;
        }

        // 存在下一阶段时通知服务器、学员端和像素流端同时进入
        if (NextStageIndex != INDEX_NONE)
        {
            RPC_MulticastEnterCourseTaskStage(CurrentTaskIndex, NextStageIndex);
        }

        // 顺序阶段事件已经处理完成
        return;
    }

    // 普通WaitEvent任务继续使用原有事件完成逻辑
    DataDrivenTaskInstance->TryCompleteByEvent(EventData);
}



void UAC_Task::RPC_ServerReportCourseEvent_Implementation(const FCourseEventData& EventData)
{
    // 在服务器上处理客户端上报的课程事件
    HandleCourseEvent(EventData);
}

// 所有客户端同步进入指定任务阶段
void UAC_Task::RPC_MulticastEnterCourseTaskStage_Implementation(int32 TaskIndex, int32 StageIndex)
{
    // 检查任务索引是否有效
    if (!RuntimeTaskInstances.IsValidIndex(TaskIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("同步进入任务阶段失败：任务索引无效，任务索引=%d，阶段索引=%d"), TaskIndex, StageIndex);
        return;
    }

    // 获取需要进入阶段的数据驱动任务
    UDataDrivenTaskItem* DataDrivenTaskInstance = Cast<UDataDrivenTaskItem>(RuntimeTaskInstances[TaskIndex]);

    // 任务不是数据驱动任务时不能进入阶段
    if (!IsValid(DataDrivenTaskInstance))
    {
        UE_LOG(LogTemp, Error, TEXT("同步进入任务阶段失败：指定任务不是数据驱动任务，任务索引=%d"), TaskIndex);
        return;
    }

    // 防止已经切换任务后收到旧任务的阶段消息
    if (CurrentTask != DataDrivenTaskInstance)
    {
        UE_LOG(LogTemp, Warning, TEXT("同步进入任务阶段已忽略：指定任务不是当前任务，任务索引=%d，阶段索引=%d"), TaskIndex, StageIndex);
        return;
    }

    // 在当前服务器或客户端进入指定阶段并执行阶段Actor命令
    DataDrivenTaskInstance->EnterTaskStage(StageIndex);
}

