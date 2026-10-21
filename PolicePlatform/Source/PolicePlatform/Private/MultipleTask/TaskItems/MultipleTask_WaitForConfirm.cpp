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

	UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] Task started: Policy=%d"),
		(int32)ConfirmPolicy);

	// 基类负责下发面板并绑定按钮回传
	Super::StartTask_Implementation();

	UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] Confirm panel pushed to %d player(s)"), ResolvePanelAudience().Num());
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
	else
	{
		// 没有确认按钮时玩家无法通过面板确认，任务可能永远无法达成
		UE_LOG(LogTemp, Warning, TEXT("[WaitForConfirm] ConfirmButtonText is empty, no confirm button added"));
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

			UE_LOG(LogTemp, Verbose, TEXT("[WaitForConfirm] Controller %s mapped to role %s"), *PC->GetName(), *RoleTag.ToString());
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

				UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] Progress: %d/%d participants confirmed (policy=AllPlayers)"), Confirmed, Participants.Num());
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

				UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] Progress: %d/%d pawns confirmed (policy=AllPlayers, fallback)"), Confirmed, AllPawns.Num());
			}
		}
		break;
	}
	case EConfirmPolicy::AnyPlayer:
		bAllMet = ConfirmedRoleTags.Num() > 0 || ConfirmedControllers.Num() > 0;

		UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] Progress: %d role(s), %d controller(s) confirmed (policy=AnyPlayer, need 1)"), ConfirmedRoleTags.Num(), ConfirmedControllers.Num());
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

		UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] Progress: %d/%d required roles confirmed (policy=SpecificRoles)"), AllConfirmed, SpecificRoleTags.Num());
		break;
	}
	case EConfirmPolicy::FirstRole_1:
		bAllMet = ConfirmedRoleTags.Contains(FName(TEXT("Role_1")));

		UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] Progress: Role_1 %s (policy=FirstRole_1)"), bAllMet ? TEXT("confirmed") : TEXT("pending"));
		break;
	}

	if (bAllMet)
	{
		UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] All confirmations received, task ending"));
		EndTask();
	}
}
