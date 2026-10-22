#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_TagMonitor.generated.h"

class APawn;

/** 标签列表包装（UPROPERTY 不支持 TMap<FName, TArray<FName>>，需用 USTRUCT 包一层） */
USTRUCT(BlueprintType)
struct FTagList
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FName> Tags;
};

/**
 * 标签监听任务：低频轮询指定身份的 Pawn 是否贴上了指定标签，满足条件即结束。
 * 用于执法记录仪开启、装备穿戴确认等「做动作 → 贴标签 → 监听」的通用场景。
 * 仅服务器执行。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_TagMonitor : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void EndTask_Implementation() override;

	/** 身份 → 要监听的标签列表（该身份 Pawn 贴上其中任意一个标签即视为满足） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TagMonitor")
	TMap<FName, FTagList> MonitoredRoles;

	/** true = 所有身份都满足才结束；false = 任意一个身份满足即结束 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TagMonitor")
	bool bRequireAllRoles = true;

	/** 轮询间隔（秒） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TagMonitor", meta = (ClampMin = "0.1"))
	float CheckInterval = 0.5f;

	/** 所有身份当前是否都已满足 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TagMonitor")
	bool AreAllRolesSatisfied() const;

	/** 某身份当前是否已满足 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TagMonitor")
	bool IsRoleSatisfied(FName RoleTag) const;

	/** 获取当前未满足的身份列表（调试/提示用） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TagMonitor")
	TArray<FName> GetUnsatisfiedRoles() const;

private:
	FTimerHandle CheckTimerHandle;

	void CheckRequirements();
	bool IsPawnTagged(APawn* Pawn, const TArray<FName>& RequiredTags) const;
};
