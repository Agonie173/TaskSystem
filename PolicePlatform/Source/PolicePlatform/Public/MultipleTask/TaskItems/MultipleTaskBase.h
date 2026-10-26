// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MultipleTaskBase.generated.h"

class UTaskSequencerComponent;
class UMultipleTaskBase;
class APawn;
class AActor;

// 任务结束时广播，携带自身，避免 Sequencer 无法区分是哪个任务结束
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskEnd, UMultipleTaskBase*, EndedTask);

/**
 * Abstract                      指定此类为抽象基类。可被继承，但不可生成对象。
 * DefaultToInstanced            指定该类的所有实例属性都默认是UPROPERTY(instanced)，即都默认创建新的实例，而不是对对象的引用。
 * EditInlineNew                 指定该类的对象可以在属性细节面板里直接内联创建，要和属性的Instanced配合。
 * CollapseCategories            在类的属性面板里隐藏所有带Category的属性，但是只对带有多个嵌套Category的属性才起作用。
 * Blueprintable                 可以在蓝图里被继承，隐含的作用也可当变量类型
 *
 * 通用任务项基类：所有具体任务类型都继承此类
 *
 * 职责：定义任务生命周期入口（StartTask/EndTask）、碰撞事件转发接口与评分上报接口。
 * 设计原因：任务实例由 Sequencer 从配表模板 DuplicateObject 生成、仅存在于服务器；
 *          本类是纯 UObject，GetWorld 依赖 Outer 链解析（Outer 为 Sequencer/GameState），
 *          因此必须由 Sequencer 注入 OwnerSequencer 并保持挂靠关系，否则世界相关逻辑失效。
 * 使用方式：子类重写 StartTask_Implementation 启动任务；完成条件达成时必须调用 EndTask()
 *          广播 OnTaskEnd 驱动 Sequencer 推进步骤——不调用 EndTask 任务永远不会结束。
 * 注意坑：EndTask 用 bTaskEnded 做幂等保护，可安全重复调用但只会广播一次；
 *         任务对象只在服务器存在，客户端调用 StartTask/EndTask 无意义。
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
	// 用途：任务启动入口（BlueprintNativeEvent，蓝图可重写）；基类实现仅复位 bTaskEnded。
	// 调用时机：由 Sequencer 推进到该步骤时调用。
	// 网络语义：仅服务器调用，任务对象不存在于客户端。
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void StartTask();
	virtual void StartTask_Implementation();

	// 结束任务
	// 用途：结束任务并广播 OnTaskEnd，是 Sequencer 推进步骤的唯一驱动。
	// 调用时机：子类在完成条件达成时调用（如定时器到期、全员确认）；可安全重复调用。
	// 网络语义：仅服务器调用；bTaskEnded 保证幂等，重复调用不会重复广播。
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void EndTask();
	virtual void EndTask_Implementation();

	// 任务的名字
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayPriority = 0))
	FName TaskName;

	// 服务器端：引用 Sequencer（由 Sequencer 在 StartTask 前注入）
	UPROPERTY()
	TObjectPtr<UTaskSequencerComponent> OwnerSequencer;

	// 是否已经结束过（防止重复广播导致 Sequencer 重复计数）
	UPROPERTY(BlueprintReadOnly, Category = "MultipleTask")
	bool bTaskEnded = false;

	// ==================== 步骤完成语义 ====================
	/**
	 * 本任务结束后是否阻塞步骤推进。
	 * 默认 true（阻塞），纯展示类非阻塞任务（如 WidgetDisplay）重写返回 false。
	 * Sequencer 的 CheckStepCompletion 只等所有 ShouldBlockStepCompletion()==true 的任务 EndTask。
	 */
	virtual bool ShouldBlockStepCompletion() const { return true; }

	// ==================== 碰撞盒驱动接口 ====================
	// OverlapBegin 事件由 Sequencer 从关卡 TriggerVolume 转发过来
	virtual void OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor);
	// OverlapEnd 事件由 Sequencer 从关卡 TriggerVolume 转发过来
	virtual void OnActorOverlapEnd(AActor* OverlappedActor, AActor* OtherActor);

	// ==================== 自定义事件接口 ====================
	// 客户端自定义事件（如语音识别结果）经 Sequencer 从 TaskPanelChannelComponent 转发过来
	virtual void OnCustomEvent(APlayerController* PC, FName EventId, const FString& Payload);

	// ==================== 辅助方法 ====================
	// 根据身份Tag查找对应的Pawn（通过Sequencer）
	// 网络语义：仅服务器有意义，依赖 Sequencer 内的身份注册表；OwnerSequencer 未注入时返回空
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MultipleTask")
	APawn* FindPawnByRoleTag(FName RoleTag) const;

	/**
	 * 上报一次评分事件（服务器权威）。
	 * 任务只给「谁 + 错误码」，扣分值由配表 FScoreRule 决定。
	 */
	// 网络语义：仅服务器调用有效；客户端（无 GameState/ScoreComponent）调用会被忽略并打印告警
	UFUNCTION(BlueprintCallable, Category = "MultipleTask|Score")
	void ReportScoreEvent(FName RoleTag, FName ErrorCode);

protected:
	// 根据Tag查找关卡中的Actor（通过Sequencer）
	AActor* FindActorByTag(FName ActorTag) const;
};
