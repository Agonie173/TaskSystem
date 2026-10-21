// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask/UI/TaskPanelTypes.h"
#include "MultipleTask_WidgetDisplay.generated.h"

class UUserWidget;
class UTaskWorldWidgetComponent;
class USceneComponent;

/**
 * 世界空间 Widget 提示任务：
 * 在每个受众玩家 Pawn 上找到带指定标签的 SceneComponent，生成 WidgetComponent 附着其上，
 * 用于头顶提示、身体标记等场景内 3D 提示（区别于走面板通道的屏幕面板）。
 *
 * 职责：解析受众 Pawn → 在其身上定位附着点（带 AttachComponentTag 的 SceneComponent）→
 *      生成 UTaskWorldWidgetComponent 并下发 Widget 类 → 按配置的生命周期移除。
 * 设计原因：纯提示不阻塞步骤推进（ShouldBlockStepCompletion 返回 false）——
 *          步骤内其他任务全部结束时 Sequencer 推进步骤，StopAllTasks 触发本任务
 *          EndTask 统一销毁 Widget；DisplayDuration>0 时可提前定时结束。
 * 使用方式：只需配置 WidgetClass / DisplayDuration / AttachComponentTag / Audience 四项；
 *          DisplayDuration=0 表示完全跟随步骤生命周期。
 * 注意坑：附着点找不到时兜底挂到 Pawn 根组件并打警告；仅服务器有效，
 *         WidgetClass 经 UTaskWorldWidgetComponent 复制到客户端重建。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_WidgetDisplay : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	// 启动任务：解析受众并生成附着 Widget（仅服务器）
	virtual void StartTask_Implementation() override;

	// 结束任务：销毁全部本实例创建的 Widget 组件后幂等广播（仅服务器，可安全重复调用）
	virtual void EndTask_Implementation() override;

	// 纯提示不阻塞步骤完成——步骤推进时随 StopAllTasks 一起销毁
	virtual bool ShouldBlockStepCompletion() const override { return false; }

	// ==================== 配置 ====================

	// 要显示的 Widget 类（玩家自行制作的 UMG 控件）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget")
	TSubclassOf<UUserWidget> WidgetClass;

	// 持续时长（秒）。0 = 不定时结束，跟随步骤生命周期（其他任务全部结束后自动移除）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget")
	float DisplayDuration = 0.f;

	// true = 按 Widget 自身期望尺寸绘制（忽略 DrawSize）；false = 按 DrawSize 绘制
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget")
	bool bDrawAtDesiredSize = false;

	// 绘制尺寸（bDrawAtDesiredSize = false 时生效；为 true 时隐藏此编辑项）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget",
		meta = (EditCondition = "!bDrawAtDesiredSize", EditConditionHides))
	FVector2D DrawSize = FVector2D(300.f, 150.f);

	// 附着目标：Pawn 身上带此 Tag 的 SceneComponent（找不到则兜底挂 Pawn 根组件）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget")
	FName AttachComponentTag;

	// 提示受众：全部参训玩家（自动剔除非玩家 Pawn）或指定身份的玩家
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget")
	EPanelAudience Audience = EPanelAudience::AllParticipants;

	// 目标身份标签（Audience=SpecificRoles 时生效）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget",
		meta = (EditCondition = "Audience == EPanelAudience::SpecificRoles", EditConditionHides))
	TArray<FName> TargetRoleTags;

protected:
	// 解析受众 Pawn 列表（All=已注册参训玩家并过滤非玩家；Specific=按身份 Tag 查 Pawn）
	TArray<APawn*> ResolveTargetPawns() const;

	// 在 Pawn 上定位附着点：优先带 AttachComponentTag 的 SceneComponent，兜底根组件
	USceneComponent* FindAttachTarget(APawn* Pawn) const;

private:
	// 定时结束句柄（DisplayDuration>0 时使用，EndTask 清除）
	FTimerHandle DisplayTimerHandle;

	// 本实例创建的所有 Widget 组件（EndTask 时统一销毁）
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTaskWorldWidgetComponent>> CreatedWidgets;
};
