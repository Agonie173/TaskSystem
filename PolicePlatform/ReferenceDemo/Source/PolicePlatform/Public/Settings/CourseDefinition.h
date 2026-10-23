// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Settings/Data.h"
#include "CourseDefinition.generated.h"

/**
 * 
 */
UCLASS()
class POLICEPLATFORM_API UCourseDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:

	// 课件编号，例如KJ5对应5
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "课件数据")
	int32 CourseId = 0;

	// 课件名称，用于编辑器中识别当前课件
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "课件数据")
	FString CourseName = TEXT("");

	// 课件包含的任务流程，按照数组顺序从上到下依次执行
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "课件数据", meta = (DisplayName = "任务流程（按照数组顺序执行）"))
	TArray<FCourseTaskDefinition> Tasks;
};
