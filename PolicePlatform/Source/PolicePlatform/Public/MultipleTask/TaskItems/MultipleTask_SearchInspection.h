// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_SearchInspection.generated.h"

class USoundBase;

USTRUCT(BlueprintType, Blueprintable)
struct FBodyPartConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName PartName;              // 部位名（如 "躯干", "左臂"）

	// Mesh 上的 Socket 名（用于定位高亮位置 / Overlap Box 放置）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName SocketName;

	// 碰撞盒 Tag（关卡里预放置的 Overlap Box，绑定到 TargetActor）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName CollisionBoxTag;
};

/**
 * 搜身检查任务：
 * 按 BodyParts 配置的顺序逐部位推进（躯干、左臂、右臂、左腿、右腿等），
 * 玩家 Pawn 与触发盒发生 OverlapBegin（由 Sequencer 从关卡触发盒转发）即完成
 * 当前部位检查，全部部位完成后结束任务。
 *
 * 判定口径：
 *  - 若当前部位的 CollisionBoxTag 为空（NAME_None），则不做触发盒过滤——
 *    任何 Pawn 碰到任何一个转发过来的触发盒都会推进该部位（宽松口径，配置时注意）；
 *    CollisionBoxTag 非空时只认 Actor Tags 命中该 Tag 的触发盒；
 *  - 触发者资格：InspectorRoleTags 为空 = 所有 Pawn 都可搜身；非空 = 只认
 *    Actor Tags 命中其一的触发者（按 Pawn 的 Tags 匹配）。
 *
 * 已知限制（空桩，如实记录）：HighlightColor 高亮（HighlightCurrentPart /
 * ClearHighlightCurrentPart）与 SearchResultText 结果文字目前只是占位，
 * 没有实际表现实现，仅保留日志与调用骨架，待蓝图/后续版本补齐。
 *
 * 网络语义：部位推进与判定只在服务器执行，客户端无表现同步（已知限制）。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_SearchInspection : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	/** 任务开始：查找目标并从第一个部位开始监控 Overlap（仅服务器） */
	virtual void StartTask_Implementation() override;
	/** Overlap 事件（Sequencer 从关卡触发盒转发）：命中当前部位触发盒即推进 */
	virtual void OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor) override;
	/** 任务结束：清理高亮（空桩）并输出结果文字 */
	virtual void EndTask_Implementation() override;

	// ==================== 目标 ====================

	// 被搜身的目标 NPC 的 Tag
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target")
	FName TargetActorTag = FName(TEXT("Suspect"));

	// ==================== 交互与部位 ====================

	// 哪些身份可以执行搜身（空=所有人都可以）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	TArray<FName> InspectorRoleTags;

	// 需要检查的部位列表（顺序即执行顺序）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BodyParts")
	TArray<FBodyPartConfig> BodyParts;

	// ==================== 表现与结果 ====================

	// 高亮颜色（当前未生效，仅占位配置，配合空桩高亮逻辑）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	FLinearColor HighlightColor = FLinearColor::Green;

	// 单部位检查完成的音效（服务器本地播放）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	USoundBase* PartCompleteSound = nullptr;

	// 全部部位完成的音效（服务器本地播放）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	USoundBase* AllCompleteSound = nullptr;

	// 搜身完成后显示的结果文字（如 "搜出管制刀具一把"）
	// 注意：当前未接入 UI 显示，空桩仅在 EndTask 打日志
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	FText SearchResultText;

private:
	// 当前检查到的部位下标（越界 = 全部完成）
	int32 CurrentPartIndex = 0;
	// 已完成的部位计数（统计用）
	int32 TotalPartsChecked = 0;

	/** StartTask 时按 TargetActorTag 缓存的被搜身目标（音效播放位置也取自它） */
	UPROPERTY()
	TObjectPtr<AActor> CachedTargetActor;

	/** 高亮当前部位（空桩：仅日志，未实现表现，留接口给蓝图/Sequencer 层） */
	void HighlightCurrentPart();

	/** 清除当前部位高亮（空桩：未实现表现） */
	void ClearHighlightCurrentPart();

	/** 推进到下一部位；全部完成后播放音效并结束任务 */
	void AdvanceToNextPart();
};
