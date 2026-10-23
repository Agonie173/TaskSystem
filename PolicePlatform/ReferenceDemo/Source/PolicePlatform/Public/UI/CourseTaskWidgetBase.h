// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/Data.h"// 提供EProcessType枚举
#include "CourseTaskWidgetBase.generated.h"

/**
 * 
 */

class UAC_Task;

// 定义提示文本根据字数使用的显示样式
UENUM(BlueprintType)
enum class ECoursePromptTextStyle : uint8
{
	// 0至40个字符使用一行样式
	OneLine UMETA(DisplayName = "一两行（0至40字）"),

	// 41至60个字符使用两行样式
	TwoLines UMETA(DisplayName = "三行（41至60字）"),

	// 61个及以上字符使用三行样式
	ThreeLines UMETA(DisplayName = "四五行（61字以上）")
};

UCLASS(Abstract, Blueprintable)
class POLICEPLATFORM_API UCourseTaskWidgetBase : public UUserWidget
{
	GENERATED_BODY()
	
public:

	// 初始化课程任务UI并传入任务配置和显示环境
	UFUNCTION(BlueprintCallable, Category = "课件任务UI")
	void InitializeCourseTaskWidget(UAC_Task* InTaskComponent, FName InEventId, FName InTargetId, bool bInTrainingMode, const FText& InPromptText, EProcessType InProcessType);

	// 玩家点击确认按钮时向任务组件上报课程事件
	UFUNCTION(BlueprintCallable, Category = "警务 | 数据驱动任务")
	void ConfirmCourseTask();

	// 确认当前任务并向任务组件上报一个整数，用于人数选择等任务
	UFUNCTION(BlueprintCallable, Category = "课件任务UI", meta = (DisplayName = "确认课程任务并上报整数"))
	void ConfirmCourseTaskWithInteger(int32 IntegerValue);


	// 本地玩家选择一个整数选项，并将选择同步给服务器
	UFUNCTION(BlueprintCallable, Category = "课件任务UI", meta = (DisplayName = "选择课程整数选项"))
	void SelectCourseIntegerOption(int32 IntegerValue);

protected:

	// 保存当前UI需要通知的任务组件
	UPROPERTY(BlueprintReadOnly, Category = "警务 | 数据驱动任务")
	UAC_Task* TaskComponent = nullptr;

	// 保存玩家确认UI时需要上报的事件ID
	UPROPERTY(BlueprintReadOnly, Category = "警务 | 数据驱动任务")
	FName EventId = NAME_None;

	// 保存玩家确认UI时需要上报的目标ID
	UPROPERTY(BlueprintReadOnly, Category = "警务 | 数据驱动任务")
	FName TargetId = NAME_None;

	// 保存当前是否为训练模式，考核模式时该值为false
	UPROPERTY(BlueprintReadOnly, Category = "课件任务UI")
	bool bIsTrainingMode = false;

	// 保存当前任务需要显示的提示文本
	UPROPERTY(BlueprintReadOnly, Category = "课件任务UI")
	FText TaskPromptText;

	// 保存当前提示文本的字符数量
	UPROPERTY(BlueprintReadOnly, Category = "课件任务UI")
	int32 TaskPromptTextLength = 0;

	// 保存当前UI对应的运行端类型
	UPROPERTY(BlueprintReadOnly, Category = "课件任务UI")
	EProcessType TaskProcessType = EProcessType::None;

	// 保存当前提示文本应该使用的一行、两行或三行样式
	UPROPERTY(BlueprintReadOnly, Category = "课件任务UI")
	ECoursePromptTextStyle PromptTextStyle = ECoursePromptTextStyle::OneLine;

	// VR端和像素流端使用世界空间UI时为true
	UPROPERTY(BlueprintReadOnly, Category = "课件任务UI")
	bool bIsWorldSpaceTaskWidget = false;

	// 通知子类蓝图课程数据已经传入，可以开始刷新模式相关的界面样式
	UFUNCTION(BlueprintImplementableEvent, Category = "课件任务UI", meta = (DisplayName = "课程任务UI初始化完成"))
	void OnCourseTaskWidgetInitialized_BIE();

	// 当前界面是否允许操作，只有任务所属的本地玩家可以操作
	UPROPERTY(BlueprintReadOnly, Category = "课件任务UI")
	bool bCanOperateTaskWidget = false;

	// 当前是否正在根据服务器同步结果刷新UI，防止SetIsChecked再次触发上报
	UPROPERTY(BlueprintReadOnly, Category = "课件任务UI")
	bool bIsApplyingIntegerSelection = false;

	// 接收任务组件广播的最新整数选择
	UFUNCTION()
	void HandleCourseIntegerSelectionChanged(int32 IntegerValue);

	// 通知蓝图刷新对应的CheckBox，-1表示全部不选
	UFUNCTION(BlueprintImplementableEvent, Category = "课件任务UI", meta = (DisplayName = "课程整数选择已更新"))
	void OnCourseIntegerSelectionChanged_BIE(int32 IntegerValue);

	// 界面销毁时解除任务组件的委托绑定
	virtual void NativeDestruct() override;

private:

	// 记录当前UI是否已经确认，避免按钮连续点击导致重复上报
	bool bHasConfirmed = false;
};

