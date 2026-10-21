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

	UE_LOG(LogTemp, Log, TEXT("[PanelBase] Panel task start: Instance=%s, Type=%d"), *PanelInstanceId.ToString(), (int32)PanelType);

	PushPanelToAudience();
}

void UMultipleTask_PanelBase::EndTask_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("[PanelBase] Panel task end: Instance=%s, HidePanel=%d"), *PanelInstanceId.ToString(), bHidePanelOnEnd ? 1 : 0);

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
		// 双保险过滤：即使非玩家 PC（像素流客户端等）被显式注册，也不给它下发面板；
		// Pawn 未占有时保留（可能之后生成），带 Pawn 但没合法玩家标签才剔除
		Participants.RemoveAll([this](const APlayerController* PC)
		{
			const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			return Pawn && !OwnerSequencer->IsPawnValidPlayer(Pawn);
		});

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
	UE_LOG(LogTemp, Log, TEXT("[PanelBase] Pushing panel to audience: %d player(s)"), ResolvePanelAudience().Num());

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

	UE_LOG(LogTemp, Log, TEXT("[PanelBase] Panel pushed to %s: PanelId=%s, Type=%d"), *PC->GetName(), *Data.PanelId.ToString(), (int32)Data.PanelType);
}

void UMultipleTask_PanelBase::HidePanelFromAudience()
{
	UE_LOG(LogTemp, Log, TEXT("[PanelBase] Hiding panel from %d bound channel(s): PanelId=%s"), BoundChannels.Num(), *PanelInstanceId.ToString());

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
		UE_LOG(LogTemp, Log, TEXT("[PanelBase] Dropped stale panel action: PC=%s, PanelId=%s (current=%s)"),
			PC ? *PC->GetName() : TEXT("null"), *PanelId.ToString(), *PanelInstanceId.ToString());
		return;
	}

	if (!ResolvePanelAudience().Contains(PC))
	{
		UE_LOG(LogTemp, Log, TEXT("[PanelBase] Dropped action from non-audience player: PC=%s"),
			PC ? *PC->GetName() : TEXT("null"));
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[PanelBase] Panel action accepted: PC=%s, Button=%s, Row=%d"),
		PC ? *PC->GetName() : TEXT("null"), *ButtonId.ToString(), RowIndex);

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

	UE_LOG(LogTemp, Verbose, TEXT("[PanelBase] Channel bound: %s"), *Channel->GetName());
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

	UE_LOG(LogTemp, Verbose, TEXT("[PanelBase] All panel channels unbound"));
}
