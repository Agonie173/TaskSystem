// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_RoleSelect.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

void UMultipleTask_RoleSelect::StartTask_Implementation()
{
	if (Roles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RoleSelect] No roles configured, task ends immediately"));
		EndTask();
		return;
	}

	Super::StartTask_Implementation();

	// 重入/调试场景：所有人可能已经选好身份
	CheckAllAssigned();
}

FTaskPanelData UMultipleTask_RoleSelect::BuildPanelData() const
{
	FTaskPanelData Data = Super::BuildPanelData();

	for (const FRoleDefinition& Role : Roles)
	{
		const bool bOccupied = OwnerSequencer && OwnerSequencer->IsRoleAssigned(Role.RoleTag);
		const FText RoleLabel = Role.DisplayName.IsEmpty() ? FText::FromName(Role.RoleTag) : Role.DisplayName;
		const FString OccupantName = bOccupied ? GetOccupantName(Role.RoleTag) : FString();

		FPanelRowData Row;
		Row.Label = RoleLabel;
		Row.Value = FText::FromString(OccupantName);
		Row.ValueColor = bOccupied ? FLinearColor(0.5f, 0.5f, 0.5f, 1.f) : FLinearColor::White;
		Data.Rows.Add(Row);

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

	if (!OwnerSequencer || !PC || !FindRole(ButtonId))
	{
		return;
	}

	if (OwnerSequencer->AssignRole(ButtonId, PC))
	{
		// 身份状态变化要同步给所有人（已占用按钮置灰）
		RefreshAllPanels();
		CheckAllAssigned();
	}
	else if (!RejectedSelectionContent.IsEmpty())
	{
		// 只对点击者显示失败提示，其他人的面板不受影响
		FTaskPanelData Data = BuildPanelData();
		Data.Content = RejectedSelectionContent;
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
