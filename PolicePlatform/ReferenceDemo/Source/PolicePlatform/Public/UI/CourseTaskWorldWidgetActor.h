// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Settings/Data.h"// 提供EProcessType枚举
#include "CourseTaskWorldWidgetActor.generated.h"


// 提前声明三维Widget组件，减少头文件之间的依赖
class UWidgetComponent;

// 提前声明UMG界面基类，用于接收DataAsset中配置的界面类型
class UUserWidget;

// 提前声明任务组件，用于接收界面发送的课程事件
class UAC_Task;

// 提前声明课程任务UI基类，用于返回三维UI内部的Widget
class UCourseTaskWidgetBase;

UCLASS()
class POLICEPLATFORM_API ACourseTaskWorldWidgetActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// 创建课件任务三维UI载体并配置默认显示参数
	ACourseTaskWorldWidgetActor();

	// Actor结束时将自身从所属学员的像素流捕获列表移除
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 创建任务界面并根据配置设置三维UI的实际宽度
	bool InitializeWorldWidget(TSubclassOf<UUserWidget> InWidgetClass, UAC_Task* InTaskComponent, FName InEventId, FName InTargetId, float InWorldWidgetWidth, bool bInTrainingMode, const FText& InPromptText, EProcessType InProcessType);

	// 获取三维WidgetComponent内部创建的任务UI
	UCourseTaskWidgetBase* GetCourseTaskWidget() const;

private:
	// 负责把UMG界面渲染到三维世界中
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "课件任务UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> WidgetComponent = nullptr;
	

};
