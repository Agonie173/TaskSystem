// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_RoleSelect.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

void UMultipleTask_RoleSelect::StartTask_Implementation()
{
	// 身份列表是核心配置：没有任何可选项时任务无法推进，直接结束避免面板空挂
	if (Roles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RoleSelect] No roles configured, task ends immediately"));
		EndTask();
		return;
	}

	Super::StartTask_Implementation();

	UE_LOG(LogTemp, Log, TEXT("[RoleSelect] Started with %d role(s), RequireAllParticipants=%d"),
		Roles.Num(), bRequireAllParticipants ? 1 : 0);

	// 重入/调试场景：所有人可能已经选好身份
	CheckAllAssigned();
}

FTaskPanelData UMultipleTask_RoleSelect::BuildPanelData() const
{
	FTaskPanelData Data = Super::BuildPanelData();

	// 每个身份生成一行占用状态 + 一个选择按钮；占用与否以 Sequencer 实时记录为准
	for (const FRoleDefinition& Role : Roles)
	{
		const bool bOccupied = OwnerSequencer && OwnerSequencer->IsRoleAssigned(Role.RoleTag);
		const FText RoleLabel = Role.DisplayName.IsEmpty() ? FText::FromName(Role.RoleTag) : Role.DisplayName;
		const FString OccupantName = bOccupied ? GetOccupantName(Role.RoleTag) : FString();

		// 状态行：显示占用者名字，占用时置灰
		FPanelRowData Row;
		Row.Label = RoleLabel;
		Row.Value = FText::FromString(OccupantName);
		Row.ValueColor = bOccupied ? FLinearColor(0.5f, 0.5f, 0.5f, 1.f) : FLinearColor::White;
		Data.Rows.Add(Row);

		// 选择按钮：ButtonId 即 RoleTag，占用后禁用并以占用者名作为禁用原因
		FTaskPanelButton Button;
		Button.ButtonId = Role.RoleTag;
		Button.Label = RoleLabel;
		Button.Description = Role.Description;
		Button.bEnabled = !bOccupied;
		Button.DisabledReason = FText::FromString(OccupantName);
		Data.Buttons.Add(Button);
	}

	return Data;
}

void UMultipleTask_RoleSelect::HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex)
{
	Super::HandlePanelAction_Implementation(PC, ButtonId, RowIndex);

	// 非法回传（无 Sequencer / 无玩家 / 按钮不是合法身份）直接丢弃
	if (!OwnerSequencer || !PC || !FindRole(ButtonId))
	{
		return;
	}

	// 服务器权威占用：成功则全员刷新并检查完成，失败只回执给点击者
	if (OwnerSequencer->AssignRole(ButtonId, PC))
	{
		// 占用回执：记录谁占用了哪个身份
		UE_LOG(LogTemp, Log, TEXT("[RoleSelect] Role %s assigned to %s"),
			*ButtonId.ToString(), *PC->GetName());

		// 进度回执：统计已占用的身份数量（只读，不影响判定）
		int32 AssignedCount = 0;
		for (const FRoleDefinition& Role : Roles)
		{
			if (OwnerSequencer->IsRoleAssigned(Role.RoleTag))
			{
				++AssignedCount;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[RoleSelect] Progress %d/%d roles assigned"),
			AssignedCount, Roles.Num());

		// 身份状态变化要同步给所有人（已占用按钮置灰）
		RefreshAllPanels();
		CheckAllAssigned();
	}
	else if (!RejectedSelectionContent.IsEmpty())
	{
		// 占用失败（身份已被抢注等）：只对点击者显示失败提示，其他人的面板不受影响
		UE_LOG(LogTemp, Log, TEXT("[RoleSelect] Role %s rejected for %s (already occupied)"),
			*ButtonId.ToString(), *PC->GetName());

		FTaskPanelData Data = BuildPanelData();
		// Content 字段已移除：把拒绝原因塞进 Rows，由外层 UI 渲染
		Data.Rows.Add(FPanelRowData{ FText::FromString(TEXT("提示")), RejectedSelectionContent });
		PushPanelDataToController(PC, Data);
	}
}

const FRoleDefinition* UMultipleTask_RoleSelect::FindRole(FName RoleTag) const
{
	for (const FRoleDefinition& Role : Roles)
	{
		if (Role.RoleTag == RoleTag)
		{
			return &Role;
		}
	}
	return nullptr;
}

FString UMultipleTask_RoleSelect::GetOccupantName(FName RoleTag) const
{
	if (!OwnerSequencer)
	{
		return FString();
	}

	if (APlayerController* PC = OwnerSequencer->GetControllerByRoleTag(RoleTag))
	{
		// 占用者以 PlayerState 上的玩家名展示（可能为空，面板侧自行兜底）
		if (const APlayerState* PS = PC->GetPlayerState<APlayerState>())
		{
			return PS->GetPlayerName();
		}
	}
	return FString();
}

bool UMultipleTask_RoleSelect::HasAnyRole(APlayerController* PC) const
{
	if (!OwnerSequencer || !PC)
	{
		return false;
	}

	for (const FRoleDefinition& Role : Roles)
	{
		if (OwnerSequencer->GetControllerByRoleTag(Role.RoleTag) == PC)
		{
			return true;
		}
	}
	return false;
}

void UMultipleTask_RoleSelect::RefreshAllPanels()
{
	PushPanelToAudience();
}

void UMultipleTask_RoleSelect::CheckAllAssigned()
{
	if (!OwnerSequencer)
	{
		return;
	}

	// 不要求所有玩家都选时，至少要等「配置的身份全部被占用」才结束，
	// 否则任务一启动就会把面板关掉，玩家根本没机会选择。
	if (!bRequireAllParticipants)
	{
		for (const FRoleDefinition& Role : Roles)
		{
			if (!OwnerSequencer->IsRoleAssigned(Role.RoleTag))
			{
				return;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[RoleSelect] All configured roles assigned, task ending"));
		EndTask();
		return;
	}

	const TArray<APlayerController*> Participants = OwnerSequencer->GetParticipants();
	if (Participants.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RoleSelect] No participants registered, waiting..."));
		return;
	}

	for (APlayerController* PC : Participants)
	{
		if (!HasAnyRole(PC))
		{
			return;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[RoleSelect] All participants assigned, task ending"));
	EndTask();
}
