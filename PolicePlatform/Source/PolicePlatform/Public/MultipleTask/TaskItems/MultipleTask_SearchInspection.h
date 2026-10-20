// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_SearchInspection.generated.h"

class USoundBase;

UENUM(BlueprintType)
enum class ESearchInteractionType : uint8
{
	Overlap,    // 用碰撞盒重叠触发（优先推荐）
	Approach,   // 靠近到一定距离触发（Tick 距离检测）
};

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
 * 依次高亮身体部位（躯干、左臂、右臂、左腿、右腿），
 * 玩家与碰撞盒重叠（OverlapBegin）即完成该部位检查
 * 全部部位完成后显示搜身结果
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_SearchInspection : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor) override;
	virtual void EndTask_Implementation() override;

	// ==================== 目标 ====================

	// 被搜身的目标 NPC 的 Tag
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target")
	FName TargetActorTag = FName(TEXT("Suspect"));

	// ==================== 交互与部位 ====================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	ESearchInteractionType InteractionType = ESearchInteractionType::Overlap;

	// 哪些身份可以执行搜身（空=所有人都可以）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	TArray<FName> InspectorRoleTags;

	// 需要检查的部位列表（顺序即执行顺序）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BodyParts")
	TArray<FBodyPartConfig> BodyParts;

	// ==================== 表现与结果 ====================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	FLinearColor HighlightColor = FLinearColor::Green;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	USoundBase* PartCompleteSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	USoundBase* AllCompleteSound = nullptr;

	// 搜身完成后显示的结果文字（如 "搜出管制刀具一把"）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	FText SearchResultText;

private:
	int32 CurrentPartIndex = 0;
	int32 TotalPartsChecked = 0;

	UPROPERTY()
	TObjectPtr<AActor> CachedTargetActor;

	void HighlightCurrentPart();
	void ClearHighlightCurrentPart();
	void AdvanceToNextPart();
};
