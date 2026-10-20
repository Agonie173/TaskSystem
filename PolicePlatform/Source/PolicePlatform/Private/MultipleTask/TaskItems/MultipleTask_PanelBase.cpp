// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_PanelBase.h"
#include "MultipleTask/UI/TaskPanelChannelComponent.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

void UMultipleTask_PanelBase::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	// 每个任务实例一个独立面板 Id，用于丢弃过期面板上的点击
	PanelInstanceId = GetFName();

	PushPanelToAudience();
}

void UMultipleTask_PanelBase::EndTask_Implementation()
{
	if (bHidePanelOnEnd)
	{
		HidePanelFromAudience();
	}
	UnbindAllChannels();

	Super::EndTask_Implementation();
}

// ==================== 面板下发 ====================

TArray<APlayerController*> UMultipleTask_PanelBase::ResolvePanelAudience() const
{
	TArray<APlayerController*> Result;

	if (!OwnerSequencer)
	{
		return Result;
	}

	if (Audience == EPanelAudience::AllParticipants)
	{
		TArray<APlayerController*> Participants = OwnerSequencer->GetParticipants();
		if (Participants.Num() > 0)
		{
			return Participants;
		}

		// 单机调试兜底：没有注册流程时用本地玩家
		if (const UWorld* World = GetWorld())
		{
			if (World->GetNetMode() == NM_Standalone)
			{
				if (APlayerController* PC = World->GetFirstPlayerController())
				{
					Result.Add(PC);
				}
			}
		}
		return Result;
	}

	for (const FName& RoleTag : TargetRoleTags)
	{
		if (APlayerController* PC = OwnerSequencer->GetControllerByRoleTag(RoleTag))
		{
			Result.AddUnique(PC);
		}
	}
	return Result;
}

void UMultipleTask_PanelBase::PushPanelToAudience()
{
	for (APlayerController* PC : ResolvePanelAudience())
	{
		PushPanelToController(PC);
	}
}

void UMultipleTask_PanelBase::PushPanelToController(APlayerController* PC)
{
	PushPanelDataToController(PC, BuildPanelData());
}

void UMultipleTask_PanelBase::PushPanelDataToController(APlayerController* PC, const FTaskPanelData& Data)
{
	if (!PC)
	{
		return;
	}

	UTaskPanelChannelComponent* Channel = UTaskPanelChannelComponent::FindOrCreateChannel(PC);
	if (!Channel)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PanelBase] No panel channel for %s"), *PC->GetName());
		return;
	}

	BindChannel(Channel);
	Channel->ShowPanel(Data);
}

void UMultipleTask_PanelBase::HidePanelFromAudience()
{
	for (const TWeakObjectPtr<UTaskPanelChannelComponent>& Weak : BoundChannels)
	{
		if (UTaskPanelChannelComponent* Channel = Weak.Get())
		{
			Channel->HidePanel(PanelInstanceId);
		}
	}
}

FTaskPanelData UMultipleTask_PanelBase::BuildPanelData() const
{
	FTaskPanelData Data;
	Data.PanelId = PanelInstanceId;
	Data.PanelType = PanelType;
	Data.Title = PanelTitle;
	Data.Content = PanelContent;
	Data.Rows = PanelRows;
	Data.Buttons = PanelButtons;
	return Data;
}

// ==================== 按钮回传 ====================

void UMultipleTask_PanelBase::HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex)
{
}

void UMultipleTask_PanelBase::HandleChannelAction(APlayerController* PC, FName PanelId, FName ButtonId, int32 RowIndex)
{
	// 丢弃不属于本次任务的过期面板点击
	if (!PanelId.IsNone() && PanelId != PanelInstanceId)
	{
		return;
	}

	if (!ResolvePanelAudience().Contains(PC))
	{
		return;
	}

	HandlePanelAction(PC, ButtonId, RowIndex);
}

// ==================== 通道绑定 ====================

void UMultipleTask_PanelBase::BindChannel(UTaskPanelChannelComponent* Channel)
{
	if (!Channel)
	{
		return;
	}

	for (const TWeakObjectPtr<UTaskPanelChannelComponent>& Weak : BoundChannels)
	{
		if (Weak.Get() == Channel)
		{
			return;
		}
	}

	Channel->OnPanelActionSubmitted.AddDynamic(this, &UMultipleTask_PanelBase::HandleChannelAction);
	BoundChannels.Add(Channel);
}

void UMultipleTask_PanelBase::UnbindAllChannels()
{
	for (const TWeakObjectPtr<UTaskPanelChannelComponent>& Weak : BoundChannels)
	{
		if (UTaskPanelChannelComponent* Channel = Weak.Get())
		{
			Channel->OnPanelActionSubmitted.RemoveDynamic(this, &UMultipleTask_PanelBase::HandleChannelAction);
		}
	}
	BoundChannels.Reset();
}
