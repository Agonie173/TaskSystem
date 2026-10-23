#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Settings/Data.h"
#include "TaskItem.generated.h"

class APPPlayerBase;
class UAC_Task;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskCompleted, FTaskResult, Result);

UCLASS(Blueprintable, BlueprintType)
class POLICEPLATFORM_API UTaskItem : public UObject
{
    GENERATED_BODY()


    
public:
    UTaskItem();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务 | 任务")
    FTaskData TaskData;

    UPROPERTY(BlueprintReadOnly, Category = "警务 | 任务")
    APPPlayerBase* OwnerPlayer = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "警务 | 任务")
    UAC_Task* OwnerAC_Task = nullptr;

    //编辑器下 从第一个该值为true的任务开始执行 方便测试
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务 | 任务")
    bool bTestStart = false;

    //是否是考核任务，即计入打分分数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务 | 任务")
    bool IsKeyTask = false;
    
    //如果为真时，则代表该任务只在训练模式下有效，考核模式下会自动跳过该任务
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务 | 任务")
    bool bOnlyXunLianModeVaild = false;
    
    //延时多久出发下一个任务，默认也需要延时下
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务 | 任务")
    float NextTaskInterval = 0.6f;
    
    //训练模式提示UI的纹理图片
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务 | 任务")
    UTexture2D*  XunLianUIImage = nullptr;
    
    //考核模式提示UI的纹理图片
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务 | 任务")
    UTexture2D*  KaoHeUIImage = nullptr;
    
    //任务执行结果
    UPROPERTY(BlueprintReadOnly, Category = "警务 | 任务")
    FTaskResult TaskResult;
    
    //任务结束回调
    UPROPERTY(BlueprintAssignable, Category = "警务 | 任务")
    FOnTaskCompleted OnTaskCompleted;
    
    //任务结束，只应该在服务器上调用
    UFUNCTION()
    void EndTask(ETaskResult LResultType, const FString& LYYSpeak, const FString& LRecordText);

    //任务开始，蓝图里实现
    UFUNCTION(BlueprintImplementableEvent, Category = "警务 | 任务")
    void OnTaskStart_BIE();
 
    //任务结束，蓝图里实现
    UFUNCTION(BlueprintImplementableEvent, Category = "警务 | 任务")
    void OnTaskEnd_BIE(FTaskResult LTaskResult);

    //任务初始化，蓝图里实现
    UFUNCTION(BlueprintImplementableEvent, Category = "警务 | 任务")
    void OnTaskInit_BIE();
};
