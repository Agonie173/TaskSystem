// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Settings/Data.h"
#include "CourseLocalActorCommandInterface.generated.h"

// 声明课程本地Actor自定义命令接口
UINTERFACE(BlueprintType, Blueprintable)
class POLICEPLATFORM_API UCourseLocalActorCommandInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class POLICEPLATFORM_API ICourseLocalActorCommandInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	// 通知本地Actor执行DataAsset配置的自定义命令
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "警务 | 课程本地Actor")
	void ExecuteCourseLocalActorCustomCommand(const FCourseLocalActorCommandDefinition& CommandDefinition);
};

