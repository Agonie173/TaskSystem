#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Tools/TaskItem.h"
#include "Settings/Data.h"// 提供课程事件数据结构FCourseEventData
#include "TimerManager.h"// 提供课程本地Actor动作延迟计时器
#include "AC_Task.generated.h"

class APPPlayerBase;
class UAC_XSL;

// 声明课件数据资产类型，供任务组件保存课件配置
class UCourseDefinition;

class UPrimitiveComponent;

//训练结束委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllTasksCompleted);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStartNewTask, int32, TaskIndex, FTaskResult, PreviousTaskResult);

//训练暂停委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrainPaused, bool, bPause);

//训练用时委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrainTimeChanged, int32, CurrentTime);

// 当服务器同步的整数选择发生变化时通知本地UI刷新
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCourseIntegerSelectionChanged, int32, IntegerValue);

// 当前课程任务的装备选择UI权限发生变化
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCourseEquipmentSelectionPermissionChanged, bool, bCanOpen);

// 当前课程任务的语音识别权限发生变化
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCourseSpeechRecognitionPermissionChanged, bool, bCanRecognize);

UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class POLICEPLATFORM_API UAC_Task : public UActorComponent
{
    GENERATED_BODY()

    UPROPERTY()
    UTaskItem* CurrentTask;
    
public:
    UAC_Task();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    FTimerHandle NextTaskTimerHandle;
    FTimerHandle TrainTimeTimerHandle;
    
    //在蓝图里把任务object放到该数组里，会按数组顺序执行任务
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务 | 任务")
    TArray<TSubclassOf<UTaskItem>> TaskClassList;

    // 保存当前课件使用的数据资产，例如KJ5使用DA_KJ5_Course
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "警务 | 数据驱动任务")
    UCourseDefinition* CourseDefinition = nullptr;

    //所有任务结束，也就是训练结束回调
    UPROPERTY(BlueprintAssignable, Category = "警务 | 任务")
    FOnAllTasksCompleted OnAllTasksCompleted;

    //新的任务开始回调
    UPROPERTY(BlueprintAssignable, Category = "警务 | 任务")
    FOnStartNewTask OnStartNewTask;

    //训练暂停/恢复状态变化委托
    UPROPERTY(BlueprintAssignable, Category = "警务 | 任务")
    FOnTrainPaused OnTrainPaused;
    
    //训练用时变化委托
    UPROPERTY(BlueprintAssignable, Category = "警务 | 任务")
    FOnTrainTimeChanged OnTrainTimeChanged;
    
    //当前任务的序号
    UPROPERTY(ReplicatedUsing=OnRep_CurrentTaskIndex, BlueprintReadOnly, Category = "警务 | 任务")
    int32 CurrentTaskIndex = -1;
    
    //任务流程是否还在跑
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "警务 | 任务")
    bool bIsRunning = false;
    
    // 训练是否暂停中，客户端收到新状态时自动执行暂停或恢复处理
    UPROPERTY(ReplicatedUsing = OnRep_TrainPaused, BlueprintReadOnly, Category = "警务 | 任务")
    bool bTrainPaused = false;

    //当前训练用时（秒）
    UPROPERTY(ReplicatedUsing=OnRep_TrainTime, BlueprintReadOnly, Category = "警务 | 任务")
    int32 TrainTime = 0;
    
    //违规操作
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "警务 | 任务")
    bool bIsGetOutOfLine = false;
    
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "警务 | 任务")
    APPPlayerBase* OwnerPlayer = nullptr;
    
    //关键考核任务成绩数组
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "警务 | 任务")
    TArray<FTaskResult> TaskResultArray;
    
    //开始走任务流程
    UFUNCTION(BlueprintCallable, Category = "警务 | 任务")
    void StartTasks();

    // 上报不携带数值的普通课程事件，用于警情介绍等确认类任务
    UFUNCTION(BlueprintCallable, Category = "警务 | 数据驱动任务")
    void ReportCourseEvent(FName EventId, FName TargetId);

    // 上报携带整数的课程事件，用于出警人数等选择类任务
    UFUNCTION(BlueprintCallable, Category = "课件任务")
    void ReportCourseEventWithInteger(FName EventId, FName TargetId, int32 IntegerValue);

    //只应在服务器上运行
    UFUNCTION()
    void StartNextTask(FTaskResult LPreviousTaskResult);

    //获取到当前的任务
    UFUNCTION(BlueprintCallable, Category = "警务 | 任务")
    UTaskItem* GetCurrentTask() const;

    UFUNCTION()
    void OnRep_CurrentTaskIndex();

    UFUNCTION()
    void OnRep_TrainTime();

    // 客户端收到服务器同步的暂停状态时统一应用暂停或恢复
    UFUNCTION()
    void OnRep_TrainPaused();
    
    //服务器通知结束当前任务
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "警务 | 任务")
    void RPC_ServerEndCurrentTask(ETaskResult LResultType, const FString& LYYSpeak, const FString& LRecordText, const FString& LTaskName);

    //异常结束任务流程，比如违规使用了警械
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "警务 | 任务")
    void RPC_ServerEndTrain();
    
    //服务器通知客户端开始新任务
    UFUNCTION(NetMulticast, Reliable)
    void RPC_MulticastStartNewTask(int32 TaskIndex, FTaskResult PreviousTaskResult);

    // 只允许服务器设置目标学员的训练暂停状态
    void SetTrainPaused(bool bPause);
    
    
    UFUNCTION()
    void HandleNewTaskStarted(int32 TaskIndex, FTaskResult PreviousTaskResult);
    
    //生成任务object
    UFUNCTION()
    void SpawnTaskObject();
    
    //获取像素流端控制器下的组件，只应该在服务器上获取
    UFUNCTION(BlueprintPure, Category = "警务 | 任务")
    UAC_XSL* GetAC_XSL();

    // 上报选择过程中的整数变化，只允许拥有该玩家的本地客户端调用
    UFUNCTION(BlueprintCallable, Category = "课件任务")
    void ReportCourseIntegerSelection(int32 IntegerValue);

    // 判断玩家选择的目标是否为当前任务的正确目标，正确时自动上报课程事件
    UFUNCTION(BlueprintCallable, Category = "警务 | 数据驱动任务", meta = (DisplayName = "尝试提交当前课程目标"))
    bool TrySubmitCurrentCourseTarget(FName SelectedTargetId);

    // 获取当前正在执行的数据驱动任务目标ID
    UFUNCTION(BlueprintPure, Category = "警务 | 数据驱动任务", meta = (DisplayName = "获取当前课程目标ID"))
    FName GetCurrentCourseTargetId() const;

    // 当整数选择同步到当前客户端时触发，用于刷新本地UI
    UPROPERTY(BlueprintAssignable, Category = "课件任务")
    FOnCourseIntegerSelectionChanged OnCourseIntegerSelectionChanged;

    // 获取服务器同步的当前整数选择
    UFUNCTION(BlueprintPure, Category = "课件任务")
    int32 GetCurrentIntegerSelection() const
    {
        // 返回当前同步的整数选择，-1表示尚未选择
        return CurrentIntegerSelection;
    }

    // 数据驱动任务开始时通知蓝图执行对应的场景表现
    UFUNCTION(BlueprintImplementableEvent, Category = "警务 | 数据驱动任务", meta = (DisplayName = "课程任务开始动作"))
    void OnCourseTaskStartAction_BIE(FName StartActionId);

    // 任务阶段黑闪状态变化，用于像素流端控制黑色遮罩
    UFUNCTION(BlueprintImplementableEvent, Category = "警务 | 数据驱动任务", meta = (DisplayName = "课程任务阶段黑闪"))
    void OnCourseTaskStageFade_BIE(bool bFadeToBlack, float FadeDuration);

    // 数据驱动任务结束后通知蓝图执行场景表现
    UFUNCTION(BlueprintImplementableEvent, Category = "警务 | 数据驱动任务", meta = (DisplayName = "课程任务结束动作"))
    void OnCourseTaskCompletionAction_BIE(FName CompletionActionId, ETaskResult CompletionResult);

    // 设置当前任务是否允许打开装备选择UI
    void SetCourseEquipmentSelectionUIAllowed(bool bAllowed);

    // 判断当前本地PC或VR学员是否可以使用课程装备选择功能
    UFUNCTION(BlueprintPure, Category = "警务 | 数据驱动任务", meta = (DisplayName = "能否打开课程装备选择UI"))
    bool CanOpenCourseEquipmentSelectionUI() const;

    // 保存当前学员在本次课程中已经启用的装备ID
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "警务 | 课程装备")
    TArray<FName> ActiveCourseEquipmentIds;

    // 设置当前学员指定课程装备的启用状态
    UFUNCTION(BlueprintCallable, Category = "警务 | 课程装备", meta = (DisplayName = "设置课程装备启用状态"))
    bool SetCourseEquipmentActive(FName EquipmentId, bool bActive);

    // 处理一次课程装备选择，并返回装备选择UI是否可以关闭
    UFUNCTION(BlueprintCallable, Category = "警务 | 课程装备", meta = (DisplayName = "提交课程装备选择"))
    bool SubmitCourseEquipmentSelection(FName EquipmentId);

    // 判断当前学员指定课程装备是否已经启用
    UFUNCTION(BlueprintPure, Category = "警务 | 课程装备", meta = (DisplayName = "课程装备是否启用"))
    bool IsCourseEquipmentActive(FName EquipmentId) const;

    // 装备选择UI权限发生变化，变为false时用于自动关闭已经打开的UI
    UPROPERTY(BlueprintAssignable, Category = "警务 | 数据驱动任务")
    FOnCourseEquipmentSelectionPermissionChanged OnCourseEquipmentSelectionPermissionChanged;

    // 记录VR手当前进入的课程实体目标
    UFUNCTION(BlueprintCallable, Category = "警务 | 数据驱动任务", meta = (DisplayName = "进入课程VR目标"))
    void EnterCourseVRTarget(UPrimitiveComponent* HandComponent, FName TargetId);

    // 清除VR手当前离开的课程实体目标
    UFUNCTION(BlueprintCallable, Category = "警务 | 数据驱动任务", meta = (DisplayName = "离开课程VR目标"))
    void LeaveCourseVRTarget(UPrimitiveComponent* HandComponent, FName TargetId);

    // 提交指定VR手当前触碰的课程目标
    UFUNCTION(BlueprintCallable, Category = "警务 | 数据驱动任务", meta = (DisplayName = "提交当前触碰的课程VR目标"))
    bool TrySubmitTouchedCourseVRTarget(UPrimitiveComponent* HandComponent);

    // 统一处理VR玩家扣动扳机时当前触碰的课程目标
    UFUNCTION(BlueprintCallable, Category = "警务 | 数据驱动任务", meta = (DisplayName = "处理课程VR目标触发"))
    void HandleCourseVRTargetTrigger(UPrimitiveComponent* HandComponent);

    // 将一次语音识别文本上报给服务器
    UFUNCTION(BlueprintCallable, Category = "警务 | 数据驱动任务", meta = (DisplayName = "上报课程语音识别文本"))
    void ReportCourseSpeechText(int32 SpeechTaskIndex, const FString& RecognizedText);

    // 设置当前任务是否允许语音识别
    void SetCourseSpeechRecognitionAllowed(bool bAllowed);

    // 判断当前本地学员是否可以进行课程语音识别
    UFUNCTION(BlueprintPure, Category = "警务 | 数据驱动任务", meta = (DisplayName = "能否进行课程语音识别"))
    bool CanUseCourseSpeechRecognition() const;

    // 语音识别权限变化，任务超时时用于强制停止录音
    UPROPERTY(BlueprintAssignable, Category = "警务 | 数据驱动任务")
    FOnCourseSpeechRecognitionPermissionChanged OnCourseSpeechRecognitionPermissionChanged;

    // 设置当前正式开始的任务允许使用的PC输入
    void SetActiveCoursePCInputId(FName InputId);

    // 尝试使用PC通用输入完成当前课程任务
    UFUNCTION(BlueprintCallable, Category = "警务 | 数据驱动任务", meta = (DisplayName = "触发当前课程PC输入"))
    bool TryTriggerCoursePCInput(FName InputId);

    // 根据任务配置为所属学员和像素流客户端生成本地场景Actor
    void SpawnCourseLocalActors(const TArray<FCourseLocalActorSpawnDefinition>& SpawnDefinitions);

    // 设置当前正式开始的任务允许使用的VR输入
    void SetActiveCourseVRInputId(FName InputId);

    // 尝试使用VR通用输入完成当前课程任务
    UFUNCTION(BlueprintCallable, Category = "警务 | 数据驱动任务", meta = (DisplayName = "触发当前课程VR输入"))
    bool TryTriggerCourseVRInput(FName InputId);

    // 删除指定ID的课程本地Actor
    void RemoveCourseLocalActor(FName ActorId);

    // 执行任务配置的全部本地Actor动作
    void ExecuteCourseLocalActorActions(const TArray<FCourseLocalActorActionDefinition>& ActionDefinitions);

    // 执行任务配置的全部本地Actor通用命令
    void ExecuteCourseLocalActorCommands(const TArray<FCourseLocalActorCommandDefinition>& CommandDefinitions);

    // 清理当前任务尚未执行的本地Actor延时动作
    void ClearCourseLocalActorActionDelayTimers();

protected:
    // 通知蓝图执行当前VR目标自身的交互功能
    UFUNCTION(BlueprintImplementableEvent, Category = "警务 | 数据驱动任务", meta = (DisplayName = "课程VR目标被触发"))
    void OnCourseVRTargetTriggered_BIE(FName TargetId, UPrimitiveComponent* HandComponent);

    // 保存当前任务的整数选择，服务器修改后同步到其他客户端
    UPROPERTY(ReplicatedUsing = OnRep_CurrentIntegerSelection)
    int32 CurrentIntegerSelection = -1;

    // 客户端将玩家选择的整数发送给服务器
    UFUNCTION(Server, Reliable)
    void RPC_ServerSetCourseIntegerSelection(int32 IntegerValue);

    // 在客户端收到最新选择值后通知本地UI
    UFUNCTION()
    void OnRep_CurrentIntegerSelection();

    // 保存服务器权威的装备选择UI权限，只同步给所属学员客户端
    UPROPERTY(ReplicatedUsing = OnRep_CourseEquipmentSelectionUIAllowed)
    bool bCourseEquipmentSelectionUIAllowed = false;

    // 所属学员客户端收到装备选择UI权限后通知本地蓝图
    UFUNCTION()
    void OnRep_CourseEquipmentSelectionUIAllowed();

    // 保存左右手当前触碰的课程目标ID，不需要网络复制
    UPROPERTY()
    TMap<UPrimitiveComponent*, FName> TouchedCourseVRTargets;

    // 保存当前任务是否允许本地学员进行语音识别
    UPROPERTY()
    bool bCourseSpeechRecognitionAllowed = false;

    // 保存当前正式开始的任务允许使用的PC输入ID
    FName ActiveCoursePCInputId = NAME_None;

    // 判断当前进程是否应该为这个任务所属学员生成本地场景Actor
    bool ShouldCreateCourseLocalActors() const;

    // 清理这个任务组件已经生成的全部本地场景Actor
    void ClearCourseLocalActors();

    // 保存这个学员在当前客户端生成的场景Actor
    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<AActor>> SpawnedCourseLocalActors;

    // 保存当前任务允许使用的VR输入ID
    FName ActiveCourseVRInputId = NAME_None;

    // 立即执行一项课程本地Actor动作
    void ExecuteSingleCourseLocalActorAction(const FCourseLocalActorActionDefinition& ActionDefinition);

    // 根据Actor动作中的路线配置启动通用路线移动组件
    void ExecuteCourseLocalActorPathMove(AActor* TargetActor, const FCourseLocalActorActionDefinition& ActionDefinition);

    // 立即执行一条本地Actor通用命令
    void ExecuteSingleCourseLocalActorCommand(const FCourseLocalActorCommandDefinition& CommandDefinition);

    // 保存当前任务创建的本地Actor动作延时计时器
    TArray<FTimerHandle> CourseLocalActorActionDelayTimerHandles;

private:
    //根据任务数组生成的任务实例数组
    UPROPERTY()
    TArray<UTaskItem*> RuntimeTaskInstances;
    
    //bool bIsFirstTaskAfterSkip = false;
    
    //像素流端控制器
    UPROPERTY()
    UAC_XSL* CachedXSLComponent = nullptr;

    // 将客户端产生的完整课程事件可靠地发送到服务器
    UFUNCTION(Server, Reliable)
    void RPC_ServerReportCourseEvent(const FCourseEventData& EventData);

    // 服务器通知所有客户端进入指定任务阶段
    UFUNCTION(NetMulticast, Reliable)
    void RPC_MulticastEnterCourseTaskStage(int32 TaskIndex, int32 StageIndex);

    // 在服务器上处理完整的课程事件数据
    void HandleCourseEvent(const FCourseEventData& EventData);

    // 根据当前网络权限直接处理事件或将事件发送到服务器
    void SubmitCourseEvent(const FCourseEventData& EventData);

    // 将客户端语音识别文本可靠发送给服务器
    UFUNCTION(Server, Reliable)
    void RPC_ServerReportCourseSpeechText(int32 SpeechTaskIndex, const FString& RecognizedText);

    // 在服务器上处理语音识别文本
    void HandleCourseSpeechText(int32 SpeechTaskIndex, const FString& RecognizedText);

    // 将拥有该玩家的客户端提交的装备状态发送到服务器
    UFUNCTION(Server, Reliable)
    void RPC_ServerSetCourseEquipmentActive(int32 EquipmentTaskIndex, FName EquipmentId, bool bActive);

    // 在服务器或单机中真正修改课程装备状态
    bool ApplyCourseEquipmentState(FName EquipmentId, bool bActive);

    // 统一把当前暂停状态应用到计时器、角色移动和蓝图表现
    void ApplyTrainPausedState();

    // 保存暂停前的角色移动模式，恢复训练时还原
    uint8 SavedTrainMovementMode = 0;

    // 保存暂停前的角色自定义移动模式
    uint8 SavedTrainCustomMovementMode = 0;

    // 标记当前是否已经保存过暂停前的移动模式
    bool bHasSavedTrainMovementMode = false;

    // 为本地PC学员打开成绩页面需要的鼠标和UI输入
    void PrepareLocalPCResultInputMode();
};
