// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Sound/SoundBase.h"// 提供音频资产类型
#include "Settings/Data.h"// 提供课程任务数据结构
#include "CourseLocalActorActionInterface.generated.h"

// This class does not need to be modified.
// 
// 声明可由蓝图实现的课程本地Actor动作接口
UINTERFACE(BlueprintType, Blueprintable)
class POLICEPLATFORM_API UCourseLocalActorActionInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class POLICEPLATFORM_API ICourseLocalActorActionInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	// 通知本地Actor执行完整动作配置
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "警务 | 课程本地Actor")
	void PlayCourseLocalActorAction(const FCourseLocalActorActionDefinition& ActionDefinition);

};
