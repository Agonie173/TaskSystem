// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTask_PanelBase.h"
#include "MultipleTask_RoleSelect.generated.h"

/** 可选择的身份定义 */
USTRUCT(BlueprintType)
struct FRoleDefinition
{
	GENERATED_BODY()

	// 身份 Tag（会贴到选择该身份的玩家 Pawn 上，如 "Role_1"）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	FName RoleTag;

	// 显示名称（如"1号 - 指挥"）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	FText DisplayName;

	// 职责说明
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	FText Description;
};

/**
 * 身份选择任务（04 步）：
 * 向所有参训玩家下发身份选择面板，服务器权威分配，身份不可重复占用。
 * 已被占用的按钮置灰；选择成功后同步刷新所有人的面板。
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_RoleSelect : public UMultipleTask_PanelBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual FTaskPanelData BuildPanelData() const override;
	virtual void HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex) override;

	// 可选身份列表
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RoleSelect")
	TArray<FRoleDefinition> Roles;

	// true=所有参训玩家都选到身份才结束；false=任意一人选到即结束
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RoleSelect")
	bool bRequireAllParticipants = true;

	// 选择已被占用的身份时，给该玩家的提示文案（留空则只刷新面板）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RoleSelect")
	FText RejectedSelectionContent;

private:
	const FRoleDefinition* FindRole(FName RoleTag) const;

	FString GetOccupantName(FName RoleTag) const;

	bool HasAnyRole(APlayerController* PC) const;

	void RefreshAllPanels();

	void CheckAllAssigned();
};
