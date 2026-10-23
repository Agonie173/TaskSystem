// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tools/TaskItem.h"
#include "Settings/Data.h"
#include "TimerManager.h"// 提供FTimerHandle
#include "DataDrivenTaskItem.generated.h"


 // 声明数据驱动任务UI基类
class UCourseTaskWidgetBase;
// 提前声明音频组件，避免在头文件中包含完整的音频组件头文件
class UAudioComponent;
// 提前声明三维任务UI载体，避免在头文件中包含完整Actor头文件
class ACourseTaskWorldWidgetActor;
// 提前声明信息子系统
class UInfoSubsystem;


 // 将数据资产中的任务定义转换成旧任务系统可以执行的任务对象
UCLASS(BlueprintType)
class POLICEPLATFORM_API UDataDrivenTaskItem : public UTaskItem
{
	GENERATED_BODY()
	
public:

	// 根据数据资产中的一条任务定义初始化当前运行时任务
	void InitializeFromDefinition(const FCourseTaskDefinition& InDefinition);

	// 根据任务定义中的触发类型启动数据驱动任务
	void StartDataDrivenTask();

	// 进入指定任务阶段并执行该阶段配置的本地Actor命令
	void EnterTaskStage(int32 StageIndex);

	// 任务结束时移除当前任务创建的UI并恢复玩家输入
	void EndDataDrivenTask();

	// 尝试使用场景上报的事件完成当前任务，匹配成功时返回true
	bool TryCompleteByEvent(const FCourseEventData& EventData);

	// 尝试使用课程事件推进顺序阶段，成功时返回下一阶段索引，最后阶段完成时返回INDEX_NONE
	bool TryAdvanceByStageEvent(const FCourseEventData& EventData, int32& OutNextStageIndex);

	// 获取WaitEvent任务或顺序阶段当前等待的事件ID和目标ID
	bool GetCurrentCourseEventCondition(FName& OutEventId, FName& OutTargetId) const;

	// 保存最终完成当前任务的课程事件，后续用于读取玩家选择的协同警员数量
	UPROPERTY(Transient)
	FCourseEventData CompletionEventData;

	// 保存当前运行时任务对应的原始数据定义
	UPROPERTY(BlueprintReadOnly, Category = "警务 | 数据驱动任务")
	FCourseTaskDefinition Definition;

	// 根据DataAsset中的锚点ID在场景中生成三维任务UI
	void ShowWorldTaskWidget();

	// 销毁当前任务创建的三维任务UI
	void RemoveWorldTaskWidget();

	// 处理一次语音识别文本，识别齐全部关键词时完成任务
	bool TryCompleteBySpeechText(const FString& RecognizedText);

	// 判断当前任务是否仍然可以接受按钮、语音、碰撞和VR输入提交
	bool CanAcceptCourseSubmission() const;

protected:

	// 正式显示任务UI、播放旁白、开放输入并启动任务触发逻辑
	void StartDataDrivenTaskContent();

	// 开始指定阶段的黑闪
	void StartTaskStageBlackFade(int32 StageIndex);

	// 画面完全变黑后执行阶段Actor命令
	void HandleTaskStageFadeOutFinished(int32 StageIndex);

	// 执行指定阶段配置的Actor命令
	void ExecuteTaskStageCommands(int32 StageIndex);

	// 结束黑屏保持并恢复画面
	void FinishTaskStageBlackFade(int32 StageIndex);

	// 清理阶段黑闪使用的计时器
	void ResetTaskStageBlackFadeTimers();

	// 清理任务内容延迟开始计时器并重置开始状态
	void ResetTaskContentStartDelay();

	// 保存任务内容延迟开始计时器
	FTimerHandle TaskContentStartDelayTimerHandle;

	// 等待阶段画面完全变黑的计时器
	FTimerHandle TaskStageFadeOutTimerHandle;

	// 阶段画面全黑保持计时器
	FTimerHandle TaskStageBlackHoldTimerHandle;

	// 当前任务的UI、旁白和交互是否已经正式开始
	bool bHasStartedTaskContent = false;

	// 获取当前客户端的信息子系统
	UInfoSubsystem* GetCourseAudioInfoSubsystem() const;

	// 判断当前进程是否需要创建这个学员的任务声音
	bool ShouldCreateCourseTaskAudio() const;

	// 获取这个学员的任务声音当前应使用的音量
	float GetCourseTaskAudioVolume() const;

	// 将任务声音注册到像素流声音路由
	void RegisterCourseTaskAudio(UAudioComponent* AudioComponent) const;

	// 将已经结束的任务声音从像素流声音路由注销
	void UnregisterCourseTaskAudio(UAudioComponent* AudioComponent) const;

	// 播放当前任务在DataAsset中配置的播报声音
	void PlayTaskNarration();

	// 停止并清理当前任务正在播放的播报声音
	void StopTaskNarration();

	// 播放当前任务配置的结束声音
	void PlayTaskCompletionSound();

	// 等待结束声音播放完成后执行任务结束动作
	void ScheduleTaskCompletionAction();

	// 执行当前任务配置的结束动作
	void ExecuteTaskCompletionAction();

	// 保存任务结束动作的延时计时器
	FTimerHandle CompletionActionTimerHandle;

	// 保存当前任务正在播放的音频组件，任务结束时用于停止播报
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveNarrationAudio = nullptr;

	// 保存当前任务已经创建的UI实例
	UPROPERTY()
	UCourseTaskWidgetBase* ActiveTaskWidget = nullptr;

	// 保存当前任务生成的三维UI Actor，任务结束时用于销毁
	UPROPERTY(Transient)
	TObjectPtr<ACourseTaskWorldWidgetActor> ActiveWorldWidgetActor = nullptr;

	// 启动服务器权威的语音任务倒计时
	void StartSpeechKeywordTask();

	// 语音任务倒计时结束时按超时完成任务
	void HandleSpeechTaskTimeout();

	// 获取当前训练或考核模式需要显示的任务文本
	FText GetTaskDisplayPromptText() const;

	// 保存语音任务的服务器倒计时
	FTimerHandle SpeechTaskTimeoutTimerHandle;

	// 保存本任务多次语音识别得到的完整文本
	FString AccumulatedSpeechText;

	// 保存当前已经识别成功的关键词
	TSet<FString> MatchedSpeechKeywords;

	// 延迟时间结束后真正执行原来的任务结束逻辑
	void FinalizeDataDrivenTaskEnd();

	// 重置并清理任务延迟结束状态
	void ResetDataDrivenTaskEndDelay();

	// 保存任务延迟结束计时器
	FTimerHandle CompletionDelayTimerHandle;

	// 当前任务是否正在等待真正结束
	bool bIsWaitingForCompletionDelay = false;

	// 当前任务是否已经真正执行过结束逻辑
	bool bHasFinalizedDataDrivenTaskEnd = false;

	// 保存延迟结束阶段播放的声音组件
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CompletionDelayAudioComponent = nullptr;

	// 判断指定功能是否允许在当前训练模式中使用
	bool IsAvailableInCurrentCourseMode(ECourseModeAvailability Availability) const;

	// 判断当前任务和模式最终是否允许打开装备选择UI
	bool ShouldAllowEquipmentSelectionUI() const;

private:

	// 记录当前任务是否已经完成，避免重复事件导致任务多次结束
	bool bHasCompleted = false;

	// 保存顺序阶段任务当前正在执行的阶段索引
	int32 CurrentTaskStageIndex = INDEX_NONE;

	// 在服务器上检查当前学员的课程装备状态并结束检查任务
	void EvaluateCourseEquipmentStateTask();

	// 为当前任务所属的本地PC学员开启或关闭课程UI输入
	void SetLocalPCCourseUIInputEnabled(bool bEnabled);

	// 根据当前运行端类型显示任务UI
	void ShowTaskWidget();

	// 如果当前是语音任务，则初始化语音任务专用UI
	void InitializeSpeechTaskWidget(UCourseTaskWidgetBase* TaskWidget);


};
