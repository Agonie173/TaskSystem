// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_WaitForConfirm.h"
#include "Components/ActorComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "MultipleTask/TaskSequencerComponent.h"

void UMultipleTask_WaitForConfirm::StartTask_Implementation()
{
	ConfirmedRoleTags.Empty();

	UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] Task started: Title=%s, Policy=%d, Visibility=%d"),
		*PanelTitle.ToString(), (int32)ConfirmPolicy, (int32)UIVisibilityMode);

	// 通知客户端弹出UI
	BroadcastShowUI();
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

	CheckCompletion();
}

void UMultipleTask_WaitForConfirm::CheckCompletion()
{
	bool bAllMet = false;

	switch (ConfirmPolicy)
	{
	case EConfirmPolicy::AllPlayers:
	{
		// 所有活跃玩家都确认
		if (OwnerSequencer)
		{
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
			bAllMet = (Confirmed >= AllPawns.Num());
		}
		break;
	}
	case EConfirmPolicy::AnyPlayer:
		bAllMet = ConfirmedRoleTags.Num() > 0;
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
		BroadcastCloseUI();
		EndTask();
	}
}

void UMultipleTask_WaitForConfirm::BroadcastShowUI()
{
	// UI 展示需要客户端渲染，这里先留接口给蓝图/Sequencer 调用
	// 实际的 UI 创建和显示可以在客户端用蓝图完成
	UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] UI should be shown on clients now: Title=%s"),
		*PanelTitle.ToString());
}

void UMultipleTask_WaitForConfirm::BroadcastCloseUI()
{
	UE_LOG(LogTemp, Log, TEXT("[WaitForConfirm] UI should be closed on clients"));
}
