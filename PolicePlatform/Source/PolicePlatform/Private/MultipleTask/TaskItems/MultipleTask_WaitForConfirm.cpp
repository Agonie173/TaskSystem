// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_WaitForConfirm.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	const FName ConfirmButtonId(TEXT("Confirm"));
}

void UMultipleTask_WaitForConfirm::StartTask_Implementation()
{
	ConfirmedRoleTags.Empty();
	ConfirmedControllers.Empty();

	UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] Task started: Title=%s, Policy=%d"),
		*PanelTitle.ToString(), (int32)ConfirmPolicy);

	// 基类负责下发面板并绑定按钮回传
	Super::StartTask_Implementation();
}

FTaskPanelData UMultipleTask_WaitForConfirm::BuildPanelData() const
{
	FTaskPanelData Data = Super::BuildPanelData();

	if (!ConfirmButtonText.IsEmpty())
	{
		FTaskPanelButton ConfirmButton;
		ConfirmButton.ButtonId = ConfirmButtonId;
		ConfirmButton.Label = ConfirmButtonText;
		Data.Buttons.Add(ConfirmButton);
	}

	return Data;
}

void UMultipleTask_WaitForConfirm::HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex)
{
	Super::HandlePanelAction_Implementation(PC, ButtonId, RowIndex);

	if (ButtonId == ConfirmButtonId)
	{
		ReportPlayerConfirmedByController(PC);
	}
}

void UMultipleTask_WaitForConfirm::ReportPlayerConfirmed(FName PlayerRoleTag)
{
	if (PlayerRoleTag.IsNone())
	{
		return;
	}

	if (!ConfirmedRoleTags.Contains(PlayerRoleTag))
	{
		ConfirmedRoleTags.Add(PlayerRoleTag);
		UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] Player with role %s confirmed. Total confirmed: %d"),
			*PlayerRoleTag.ToString(), ConfirmedRoleTags.Num());
	}

	// 同步到连接维度，保证 AllPlayers 判定可用
	if (OwnerSequencer)
	{
		if (APlayerController* PC = OwnerSequencer->GetControllerByRoleTag(PlayerRoleTag))
		{
			ConfirmedControllers.Add(PC);
		}
	}

	CheckCompletion();
}

void UMultipleTask_WaitForConfirm::ReportPlayerConfirmedByController(APlayerController* PC)
{
	if (!PC)
	{
		return;
	}

	if (!ConfirmedControllers.Contains(PC))
	{
		ConfirmedControllers.Add(PC);
		UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] Player %s confirmed. Total confirmed: %d"),
			*PC->GetName(), ConfirmedControllers.Num());
	}

	// 同步到身份维度：SpecificRoles / FirstRole_1 策略只认 ConfirmedRoleTags
	if (OwnerSequencer)
	{
		const FName RoleTag = OwnerSequencer->GetRoleTagByPawn(PC->GetPawn());
		if (!RoleTag.IsNone())
		{
			ConfirmedRoleTags.Add(RoleTag);
		}
	}

	CheckCompletion();
}

bool UMultipleTask_WaitForConfirm::IsConfirmedBy(APlayerController* PC) const
{
	if (!PC)
	{
		return false;
	}

	if (ConfirmedControllers.Contains(PC))
	{
		return true;
	}

	// 兜底：按 Pawn 上的身份 Tag 判定（兼容只传了 RoleTag 的调用方）
	if (APawn* Pawn = PC->GetPawn())
	{
		for (const FName& Tag : Pawn->Tags)
		{
			if (ConfirmedRoleTags.Contains(Tag))
			{
				return true;
			}
		}
	}
	return false;
}

void UMultipleTask_WaitForConfirm::CheckCompletion()
{
	bool bAllMet = false;

	switch (ConfirmPolicy)
	{
	case EConfirmPolicy::AllPlayers:
	{
		if (OwnerSequencer)
		{
			// 优先按已注册的参训玩家判定：监看端/观战端不会被算进来
			TArray<APlayerController*> Participants = OwnerSequencer->GetParticipants();
			if (Participants.Num() > 0)
			{
				int32 Confirmed = 0;
				for (APlayerController* PC : Participants)
				{
					if (IsConfirmedBy(PC))
					{
						Confirmed++;
					}
				}
				bAllMet = (Confirmed >= Participants.Num());
			}
			else
			{
				// 未注册参与者（单机/编辑器调试）：沿用“所有 Pawn”的旧行为
				TArray<APawn*> AllPawns = OwnerSequencer->GetAllPlayerPawns();
				int32 Confirmed = 0;
				for (APawn* Pawn : AllPawns)
				{
					if (Pawn)
					{
						for (const FName& Tag : Pawn->Tags)
						{
							if (ConfirmedRoleTags.Contains(Tag))
							{
								Confirmed++;
								break;
							}
						}
					}
				}
				bAllMet = (AllPawns.Num() > 0 && Confirmed >= AllPawns.Num());
			}
		}
		break;
	}
	case EConfirmPolicy::AnyPlayer:
		bAllMet = ConfirmedRoleTags.Num() > 0 || ConfirmedControllers.Num() > 0;
		break;

	case EConfirmPolicy::SpecificRoles:
	{
		int32 AllConfirmed = 0;
		for (const FName& Role : SpecificRoleTags)
		{
			if (ConfirmedRoleTags.Contains(Role))
			{
				AllConfirmed++;
			}
		}
		bAllMet = (AllConfirmed >= SpecificRoleTags.Num());
		break;
	}
	case EConfirmPolicy::FirstRole_1:
		bAllMet = ConfirmedRoleTags.Contains(FName(TEXT("Role_1")));
		break;
	}

	if (bAllMet)
	{
		UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] All confirmations received, task ending"));
		EndTask();
	}
}
