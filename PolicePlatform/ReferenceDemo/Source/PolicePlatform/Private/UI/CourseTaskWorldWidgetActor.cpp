// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CourseTaskWorldWidgetActor.h"
#include "Components/WidgetComponent.h"// 提供三维Widget组件的创建和配置功能
#include "UI/CourseTaskWidgetBase.h"// 提供课件任务UI的初始化和确认功能
#include "Blueprint/UserWidget.h"// 提供UUserWidget的完整类型定义
#include "Component/AC_Task.h"// 提供UAC_Task的完整类型定义，使其能够使用IsValid进行有效性判断
#include "GamePlay/PPPlayerBase.h"// 提供所属学员和像素流显示Actor注册接口

// Sets default values
ACourseTaskWorldWidgetActor::ACourseTaskWorldWidgetActor()
{
	// 关闭Tick，避免没有每帧逻辑的三维UI产生额外性能消耗
	PrimaryActorTick.bCanEverTick = false;

	// 创建负责显示UMG界面的三维Widget组件
	WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComponent"));

	// 将Widget组件设置为当前Actor的根组件
	SetRootComponent(WidgetComponent);

	// 设置Widget显示在三维世界中，而不是直接显示在玩家视口上
	WidgetComponent->SetWidgetSpace(EWidgetSpace::World);

	// 设置Widget的渲染分辨率为1920乘1080
	WidgetComponent->SetDrawSize(FVector2D(1920.0f, 1080.0f));

	// 使用固定渲染分辨率，避免UI内容变化时不断修改组件尺寸
	WidgetComponent->SetDrawAtDesiredSize(false);

	// 将Widget中心点作为Actor的位置，方便对齐场景中的三维UI锚点
	WidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));

	// 开启双面显示，避免组件朝向暂时相反时完全看不到界面
	WidgetComponent->SetTwoSided(true);

	// 将1920厘米宽的原始Widget缩放为约192厘米宽
	WidgetComponent->SetRelativeScale3D(FVector(0.1f));

	// 设置三维Widget只参与射线查询，不参与物理碰撞
	WidgetComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// 默认忽略所有碰撞通道，避免三维UI阻挡玩家和场景中的其他射线
	WidgetComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

	// 使用项目设置中的课件三维UI碰撞预设
	WidgetComponent->SetCollisionProfileName(TEXT("CourseTask3DWidget"));
}

void ACourseTaskWorldWidgetActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 获取创建当前三维UI的所属学员
	APPPlayerBase* CourseOwnerPlayer = Cast<APPPlayerBase>(GetOwner());

	// 所属学员有效时取消像素流显示注册
	if (IsValid(CourseOwnerPlayer))
	{
		CourseOwnerPlayer->UnregisterActorFromXSLShowOnly(this);
	}

	// 执行父类结束逻辑
	Super::EndPlay(EndPlayReason);
}

bool ACourseTaskWorldWidgetActor::InitializeWorldWidget(TSubclassOf<UUserWidget> InWidgetClass, UAC_Task* InTaskComponent, FName InEventId, FName InTargetId, float InWorldWidgetWidth, bool bInTrainingMode, const FText& InPromptText, EProcessType InProcessType)
{
	// 判断三维Widget组件是否创建成功
	if (!IsValid(WidgetComponent))
	{
		// 输出错误日志，说明当前Actor缺少用于显示界面的Widget组件
		UE_LOG(LogTemp, Error, TEXT("三维任务UI初始化失败：WidgetComponent无效"));
		return false;
	}

	// 读取Widget组件当前使用的渲染分辨率
	const FVector2D CurrentDrawSize = WidgetComponent->GetDrawSize();

	// 判断Widget渲染宽度是否有效，避免计算缩放比例时除以零
	if (CurrentDrawSize.X <= KINDA_SMALL_NUMBER)
	{
		// 输出错误日志，说明Widget组件的DrawSize配置不正确
		UE_LOG(LogTemp, Error, TEXT("三维任务UI初始化失败：DrawSize宽度无效"));
		return false;
	}

	// 限制三维UI最小宽度为10厘米，防止错误配置为零或者负数
	const float SafeWorldWidgetWidth = FMath::Max(InWorldWidgetWidth, 10.0f);

	// 根据目标厘米宽度和渲染分辨率计算等比例缩放值
	const float WorldWidgetScale = SafeWorldWidgetWidth / CurrentDrawSize.X;

	// 使用统一缩放保证三维任务UI不会发生横向或纵向拉伸
	WidgetComponent->SetRelativeScale3D(FVector(WorldWidgetScale));

	// 判断任务是否配置了需要显示的Widget类
	if (!InWidgetClass)
	{
		// 输出错误日志，说明DataAsset中的TaskWidgetClass为空
		UE_LOG(LogTemp, Error, TEXT("三维任务UI初始化失败：TaskWidgetClass为空"));
		return false;
	}

	// 判断负责接收课程事件的任务组件是否有效
	if (!IsValid(InTaskComponent))
	{
		// 输出错误日志，说明无法把确认事件发送给任务系统
		UE_LOG(LogTemp, Error, TEXT("三维任务UI初始化失败：任务组件无效"));
		return false;
	}

	// 将DataAsset中配置的任务界面类设置给三维Widget组件
	WidgetComponent->SetWidgetClass(InWidgetClass);

	// 根据刚设置的Widget类创建真正的UMG界面实例
	WidgetComponent->InitWidget();

	// 获取三维Widget组件内部创建出来的UMG界面实例
	UUserWidget* CreatedWidget = WidgetComponent->GetUserWidgetObject();

	// 判断UMG界面是否创建成功
	if (!IsValid(CreatedWidget))
	{
		// 输出错误日志，方便检查Widget类是否能够正常创建
		UE_LOG(LogTemp, Error, TEXT("三维任务UI初始化失败：无法创建Widget实例，UI类=%s"), *InWidgetClass->GetName());
		return false;
	}

	// 将创建出来的界面转换成项目的数据驱动任务UI基类
	UCourseTaskWidgetBase* CourseTaskWidget = Cast<UCourseTaskWidgetBase>(CreatedWidget);

	// 判断配置的界面是否正确继承CourseTaskWidgetBase
	if (!IsValid(CourseTaskWidget))
	{
		// 输出错误日志，说明KJ5任务UI的父类设置不正确
		UE_LOG(LogTemp, Error, TEXT("三维任务UI初始化失败：UI没有继承CourseTaskWidgetBase，UI=%s"), *CreatedWidget->GetName());
		return false;
	}

	// 将完整的任务显示配置传给任务UI
	CourseTaskWidget->InitializeCourseTaskWidget(InTaskComponent, InEventId, InTargetId, bInTrainingMode, InPromptText, InProcessType);

	// 获取创建当前三维任务UI的所属学员
	APPPlayerBase* CourseOwnerPlayer = Cast<APPPlayerBase>(GetOwner());

	// 所属学员有效时将当前三维UI加入该学员的像素流捕获列表
	if (IsValid(CourseOwnerPlayer))
	{
		CourseOwnerPlayer->RegisterActorToXSLShowOnly(this);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("三维任务UI注册像素流显示失败：Owner不是有效的PPPlayerBase，UI=%s"), *GetName());
	}

	// 输出成功日志，确认三维任务界面已经完成创建和初始化
	UE_LOG(LogTemp, Warning, TEXT("三维任务UI初始化成功：UI=%s，事件=%s，目标=%s"), *CreatedWidget->GetName(), *InEventId.ToString(), *InTargetId.ToString());

	// 返回成功，通知任务对象可以继续保存这个三维UI Actor
	return true;
}

UCourseTaskWidgetBase* ACourseTaskWorldWidgetActor::GetCourseTaskWidget() const
{
	// Widget组件无效时返回空
	if (!IsValid(WidgetComponent))
	{
		return nullptr;
	}

	// 返回三维Widget内部创建的任务UI
	return Cast<UCourseTaskWidgetBase>(WidgetComponent->GetUserWidgetObject());
}



