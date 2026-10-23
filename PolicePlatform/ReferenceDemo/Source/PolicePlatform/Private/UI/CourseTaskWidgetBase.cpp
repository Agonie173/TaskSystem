// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CourseTaskWidgetBase.h"
#include "Component/AC_Task.h"
#include "GameFramework/Pawn.h"// 提供APawn以及IsLocallyControlled函数

// 初始化课程任务UI并保存任务组件、事件ID、目标ID和显示配置
void UCourseTaskWidgetBase::InitializeCourseTaskWidget(UAC_Task* InTaskComponent, FName InEventId, FName InTargetId, bool bInTrainingMode, const FText& InPromptText, EProcessType InProcessType)
{
	// 如果当前界面之前绑定过任务组件，先解除旧绑定
	if (IsValid(TaskComponent))
	{
		// 防止重复初始化造成委托重复绑定
		TaskComponent->OnCourseIntegerSelectionChanged.RemoveDynamic(this, &UCourseTaskWidgetBase::HandleCourseIntegerSelectionChanged);
	}

	// 保存负责接收课程事件的任务组件
	TaskComponent = InTaskComponent;

	// 保存当前UI对应的事件ID
	EventId = InEventId;

	// 保存当前UI对应的目标ID
	TargetId = InTargetId;

	// 保存当前是否为训练模式
	bIsTrainingMode = bInTrainingMode;

	// 保存当前任务需要显示的提示文本
	TaskPromptText = InPromptText;

	// 保存当前任务界面所属的运行端类型
	TaskProcessType = InProcessType;

	// 计算当前提示文本的字符数量
	TaskPromptTextLength = TaskPromptText.ToString().Len();

	// 判断是否使用一行样式
	if (TaskPromptTextLength <= 40)
	{
		// 0至40个字符使用一行样式
		PromptTextStyle = ECoursePromptTextStyle::OneLine;
	}
	// 判断是否使用两行样式
	else if (TaskPromptTextLength <= 60)
	{
		// 41至60个字符使用两行样式
		PromptTextStyle = ECoursePromptTextStyle::TwoLines;
	}
	else
	{
		// 61个及以上字符使用三行样式
		PromptTextStyle = ECoursePromptTextStyle::ThreeLines;
	}

	// VR端和像素流端使用世界空间UI样式
	bIsWorldSpaceTaskWidget = TaskProcessType == EProcessType::VR_XYD || TaskProcessType == EProcessType::ViewPort;

	// 重置界面的确认状态
	bHasConfirmed = false;

	// 获取任务组件所属的玩家Pawn
	APawn* TaskOwnerPawn = IsValid(TaskComponent) ? Cast<APawn>(TaskComponent->GetOwner()) : nullptr;

	// 只有任务所属的本地玩家才允许操作
	bCanOperateTaskWidget = IsValid(TaskOwnerPawn) && TaskOwnerPawn->IsLocallyControlled();

	// 判断任务组件是否有效
	if (IsValid(TaskComponent))
	{
		// 绑定服务器同步的整数选择变化事件
		TaskComponent->OnCourseIntegerSelectionChanged.AddDynamic(this, &UCourseTaskWidgetBase::HandleCourseIntegerSelectionChanged);
	}

	// 输出提示文本初始化结果
	//UE_LOG(LogTemp, Warning, TEXT("任务UI初始化完成：事件=%s，运行端=%s，文本字数=%d，文本样式=%s，世界UI=%s"), *EventId.ToString(), *UEnum::GetValueAsString(TaskProcessType), TaskPromptTextLength, *UEnum::GetValueAsString(PromptTextStyle), bIsWorldSpaceTaskWidget ? TEXT("是") : TEXT("否"));

	// 通知子类蓝图刷新文本和界面样式
	OnCourseTaskWidgetInitialized_BIE();

	// 判断任务组件是否有效
	if (IsValid(TaskComponent))
	{
		// 使用已经同步的选择值初始化选择类UI
		HandleCourseIntegerSelectionChanged(TaskComponent->GetCurrentIntegerSelection());
	}
}

void UCourseTaskWidgetBase::ConfirmCourseTask()
{
	// 像素流观察端不允许确认任务
	if (!bCanOperateTaskWidget)
	{
		return;
	}

	// 当前UI已经确认时不允许再次上报
	if (bHasConfirmed)
	{
		return;
	}

	// 任务组件无效时无法上报课程事件
	if (!IsValid(TaskComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("任务UI确认失败：TaskComponent无效"));
		return;
	}

	// 训练暂停期间不允许当前任务UI继续确认
	if (TaskComponent->bTrainPaused)
	{
		UE_LOG(LogTemp, Warning, TEXT("任务UI确认已忽略：当前训练处于暂停状态，事件=%s，目标=%s"), *EventId.ToString(), *TargetId.ToString());
		return;
	}

	// 事件ID为空时无法确定需要完成哪个操作
	if (EventId.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("任务UI确认失败：EventId为空"));
		return;
	}

	// 设置已经确认，避免玩家连续点击导致重复上报
	bHasConfirmed = true;

	// 禁用当前UI，避免任务结束前再次触发按钮
	SetIsEnabled(false);

	// 输出当前UI上报的课程事件
	//UE_LOG(LogTemp, Warning, TEXT("任务UI确认：事件=%s，目标=%s"), *EventId.ToString(), *TargetId.ToString());

	// 将确认事件交给任务组件处理，客户端调用时会自动发送到服务器
	TaskComponent->ReportCourseEvent(EventId, TargetId);
}

// 确认当前任务并向任务组件上报一个整数
void UCourseTaskWidgetBase::ConfirmCourseTaskWithInteger(int32 IntegerValue)
{
	// 像素流观察端不允许确认任务
	if (!bCanOperateTaskWidget)
	{
		return;
	}

	// 判断当前界面是否已经确认，避免玩家连续点击造成重复事件
	if (bHasConfirmed)
	{
		// 已经确认过的界面不再处理后续点击
		return;
	}

	// 判断负责接收课程事件的任务组件是否有效
	if (!IsValid(TaskComponent))
	{
		// 输出错误日志，并保留按钮可用状态方便重新尝试
		UE_LOG(LogTemp, Error, TEXT("整数任务UI确认失败：任务组件无效，事件=%s，目标=%s"), *EventId.ToString(), *TargetId.ToString());
		return;
	}

	// 训练暂停期间不允许提交整数选择任务
	if (TaskComponent->bTrainPaused)
	{
		UE_LOG(LogTemp, Warning, TEXT("整数任务UI确认已忽略：当前训练处于暂停状态，事件=%s，目标=%s，整数=%d"), *EventId.ToString(), *TargetId.ToString(), IntegerValue);
		return;
	}

	// 标记当前界面已经完成确认
	bHasConfirmed = true;

	// 禁用当前界面，避免服务器处理事件期间重复点击
	SetIsEnabled(false);

	// 输出UI实际选择的整数，方便检查四个选项的赋值是否正确
	//UE_LOG(LogTemp, Warning, TEXT("整数任务UI确认：事件=%s，目标=%s，整数=%d"), *EventId.ToString(), *TargetId.ToString(), IntegerValue);

	// 将事件ID、目标ID和选择整数统一上报给任务组件
	TaskComponent->ReportCourseEventWithInteger(EventId, TargetId, IntegerValue);
}

void UCourseTaskWidgetBase::SelectCourseIntegerOption(int32 IntegerValue)
{
	// 服务器同步数据引起的CheckBox变化不能再次上报
	if (bIsApplyingIntegerSelection)
	{
		// 直接结束，防止产生循环RPC
		return;
	}

	// 像素流观察端不允许修改选择
	if (!bCanOperateTaskWidget)
	{
		// 输出警告日志，方便检查意外调用
		UE_LOG(LogTemp, Warning, TEXT("课程整数选择被拒绝：当前界面没有操作权限"));
		return;
	}

	// 判断任务组件是否有效
	if (!IsValid(TaskComponent))
	{
		// 任务组件无效时无法上报选择
		UE_LOG(LogTemp, Error, TEXT("课程整数选择失败：任务组件无效"));
		return;
	}

	// 当前出警人数任务只允许0到3
	if (IntegerValue < 0 || IntegerValue > 3)
	{
		// 拒绝非法选择
		UE_LOG(
			LogTemp,
			Error,
			TEXT("课程整数选择失败：数值超出范围，整数=%d"),
			IntegerValue);
		return;
	}

	// 立即刷新本地界面，不需要等待服务器复制回来
	HandleCourseIntegerSelectionChanged(IntegerValue);

	// 将最新选择发送给任务组件和服务器
	TaskComponent->ReportCourseIntegerSelection(IntegerValue);
}

// 接收服务器同步的最新整数选择
void UCourseTaskWidgetBase::HandleCourseIntegerSelectionChanged(int32 IntegerValue)
{
	// 标记当前正在通过代码刷新UI
	bIsApplyingIntegerSelection = true;

	// 通知蓝图按照整数值刷新四个CheckBox
	OnCourseIntegerSelectionChanged_BIE(IntegerValue);

	// 蓝图同步刷新结束，恢复玩家选择上报
	bIsApplyingIntegerSelection = false;

	// 输出UI刷新日志
	/*UE_LOG(
		LogTemp,
		Warning,
		TEXT("任务UI刷新整数选择：整数=%d，允许操作=%s"),
		IntegerValue,
		bCanOperateTaskWidget ? TEXT("是") : TEXT("否"));*/
}

void UCourseTaskWidgetBase::NativeDestruct()
{
	// 判断任务组件是否仍然有效
	if (IsValid(TaskComponent))
	{
		// 解除整数选择变化事件，防止旧UI继续收到通知
		TaskComponent->OnCourseIntegerSelectionChanged.RemoveDynamic(
			this,
			&UCourseTaskWidgetBase::HandleCourseIntegerSelectionChanged);
	}

	// 执行父类的界面销毁逻辑
	Super::NativeDestruct();
}
