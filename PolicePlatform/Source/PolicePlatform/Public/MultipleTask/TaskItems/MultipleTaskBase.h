// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MultipleTaskBase.generated.h"

class UTaskSequencerComponent;
class APawn;
class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTaskEnd);

/**
 * Abstract                      指定此类为抽象基类。可被继承，但不可生成对象。
 * DefaultToInstanced            指定该类的所有实例属性都默认是UPROPERTY(instanced)，即都默认创建新的实例，而不是对对象的引用。
 * EditInlineNew                 指定该类的对象可以在属性细节面板里直接内联创建，要和属性的Instanced配合。
 * CollapseCategories            在类的属性面板里隐藏所有带Category的属性，但是只对带有多个嵌套Category的属性才起作用。
 * Blueprintable                 可以在蓝图里被继承，隐含的作用也可当变量类型
 *
 * 通用任务项基类：所有具体任务类型都继承此类
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, CollapseCategories, Blueprintable)
class POLICEPLATFORM_API UMultipleTaskBase : public UObject
{
	GENERATED_BODY()

public:
	// 结束的委托
	UPROPERTY(BlueprintReadOnly, BlueprintAssignable, BlueprintCallable)
	FOnTaskEnd OnTaskEnd;

	// 开始任务
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void StartTask();
	virtual void StartTask_Implementation();

	// 结束任务
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void EndTask();
	virtual void EndTask_Implementation();

	// 任务的名字
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayPriority = 0))
	FName TaskName;

	// 服务器端：引用 Sequencer（由 Sequencer 在 StartTask 前注入）
	UPROPERTY()
	TObjectPtr<UTaskSequencerComponent> OwnerSequencer;

	// ==================== 碰撞盒驱动接口 ====================
	// OverlapBegin 事件由 Sequencer 从关卡 TriggerVolume 转发过来
	virtual void OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor);
	// OverlapEnd 事件由 Sequencer 从关卡 TriggerVolume 转发过来
	virtual void OnActorOverlapEnd(AActor* OverlappedActor, AActor* OtherActor);

	// ==================== 辅助方法 ====================
	// 根据身份Tag查找对应的Pawn（通过Sequencer）
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MultipleTask")
	APawn* FindPawnByRoleTag(FName RoleTag) const;

protected:
	// 根据Tag查找关卡中的Actor（通过Sequencer）
	AActor* FindActorByTag(FName ActorTag) const;
};
