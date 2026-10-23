// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Settings/Data.h"
#include "AC_CourseTransition.generated.h"

// 提前声明项目玩家基类
class APPPlayerBase;

// 提前声明玩家控制器
class APlayerController;

// 提前声明临时相机Actor
class ACameraActor;

// 负责执行任务结束后的黑屏、移动玩家和移动PC相机
UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class POLICEPLATFORM_API UAC_CourseTransition : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UAC_CourseTransition();

	// 根据任务配置开始一次课程场景过渡
	UFUNCTION(BlueprintCallable, Category = "警务 | 课程场景过渡")
	void StartCourseTransition(const FCoursePlayerTransitionDefinition& TransitionDefinition);


protected:
	// 组件开始运行时缓存所属玩家
	virtual void BeginPlay() override;

	// 组件结束运行时清理计时器和临时相机
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 保存当前组件所属的玩家
	UPROPERTY(Transient)
	TObjectPtr<APPPlayerBase> OwnerPlayer = nullptr;

	// 保存当前过渡配置
	UPROPERTY(Transient)
	FCoursePlayerTransitionDefinition ActiveTransition;

	// 保存根据中文Tag找到的目标锚点
	UPROPERTY(Transient)
	TObjectPtr<AActor> TargetAnchorActor = nullptr;

	// 保存需要移动的警车等场景Actor
	UPROPERTY(Transient)
	TObjectPtr<AActor> MovingSceneActor = nullptr;

	// 保存场景Actor移动前的世界变换
	FTransform OriginalMovingSceneActorTransform = FTransform::Identity;

	// 保存玩家或PC临时相机相对场景Actor的变换
	FTransform PreservedOccupantRelativeTransform = FTransform::Identity;

	// 标记是否成功保存了乘员相对变换
	bool bHasPreservedOccupantRelativeTransform = false;

	// 根据中文Tag查找需要移动的场景Actor
	bool FindMovingSceneActor(FName MovingSceneActorId);

	// 保存玩家或PC临时相机相对场景Actor的位置
	bool CaptureOccupantRelativeTransform();

	// 移动场景Actor并保持玩家或相机的相对位置
	void ApplySceneActorWithOccupantTransition();

	// 保存PC上车时使用的本地临时相机
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> ActivePCCameraActor = nullptr;

	// 保存等待画面全黑的计时器
	FTimerHandle TransitionTimerHandle;

	// 根据中文Tag查找目标锚点
	bool FindTargetAnchor(FName TargetAnchorId);

	// 画面变黑后执行实际移动
	void ExecuteTransitionAtBlackScreen();

	// 将VR玩家或普通PC玩家移动到目标锚点
	void ApplyPlayerTransition();

	// 将PC玩家的画面切换到目标相机位置
	void ApplyPCCameraTransition();

	// 恢复PC玩家原本的玩家相机
	void RestorePCPlayerView();

	// 开始当前本地学员端的黑屏效果
	void StartLocalFadeOut();

	// 开始当前本地学员端的画面恢复效果
	void StartLocalFadeIn();

	// 获取当前玩家对应的本地玩家控制器
	APlayerController* GetLocalPlayerController() const;

	// 判断当前运行进程是否为像素流客户端
	bool IsViewPortClient() const;
};
