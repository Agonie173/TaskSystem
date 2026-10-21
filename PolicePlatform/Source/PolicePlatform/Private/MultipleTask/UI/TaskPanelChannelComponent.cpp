// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/UI/TaskPanelChannelComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UTaskPanelChannelComponent::UTaskPanelChannelComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// ==================== 静态获取 ====================

UTaskPanelChannelComponent* UTaskPanelChannelComponent::FindChannel(APlayerController* PC)
{
	if (!PC)
	{
		return nullptr;
	}

	if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
	{
		return PS->FindComponentByClass<UTaskPanelChannelComponent>();
	}
	return nullptr;
}

UTaskPanelChannelComponent* UTaskPanelChannelComponent::FindOrCreateChannel(APlayerController* PC)
{
	if (!PC)
	{
		return nullptr;
	}

	if (UTaskPanelChannelComponent* Existing = FindChannel(PC))
	{
		return Existing;
	}

	APlayerState* PS = PC->GetPlayerState<APlayerState>();
	if (!PS || !PS->HasAuthority())
	{
		return nullptr;
	}

	// 外层框架没手动挂组件时，服务器按需创建（组件会随 PlayerState 复制到客户端）
	UTaskPanelChannelComponent* NewChannel =
		NewObject<UTaskPanelChannelComponent>(PS, TEXT("TaskPanelChannel"));
	if (!NewChannel)
	{
		return nullptr;
	}
	NewChannel->RegisterComponent();
	return NewChannel;
}

// ==================== 服务器接口 ====================

void UTaskPanelChannelComponent::ShowPanel(const FTaskPanelData& InPanel)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	CurrentPanel = InPanel;
	bPanelVisible = true;

	// 单机/监听服务器上本地玩家与服务器共用同一个对象，不会收到复制回调，手动通知一次
	if (const UWorld* World = GetWorld())
	{
		if (World->GetNetMode() != NM_DedicatedServer)
		{
			OnRep_PanelState();
		}
	}
}

void UTaskPanelChannelComponent::HidePanel(FName PanelId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	// 并行任务场景下，只关闭自己下发的面板
	if (!PanelId.IsNone() && PanelId != CurrentPanel.PanelId)
	{
		return;
	}

	bPanelVisible = false;

	if (const UWorld* World = GetWorld())
	{
		if (World->GetNetMode() != NM_DedicatedServer)
		{
			OnRep_PanelState();
		}
	}
}

// ==================== 客户端接口 ====================

void UTaskPanelChannelComponent::SubmitPanelButton(FName ButtonId, int32 RowIndex)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		// 只有本地玩家自己的通道才能向服务器提交
		APlayerController* PC = GetOwningController();
		if (PC && PC->IsLocalController())
		{
			Server_SubmitPanelButton(CurrentPanel.PanelId, ButtonId, RowIndex);
		}
		return;
	}

	SubmitPanelButtonInternal(ButtonId, RowIndex);
}

APlayerController* UTaskPanelChannelComponent::GetOwningController() const
{
	if (const APlayerState* PS = Cast<APlayerState>(GetOwner()))
	{
		return PS->GetPlayerController();
	}
	return nullptr;
}

// ==================== 网络 ====================

void UTaskPanelChannelComponent::Server_SubmitPanelButton_Implementation(FName PanelId, FName ButtonId, int32 RowIndex)
{
	// 丢弃过期面板（如面板已被替换）上的点击
	if (!PanelId.IsNone() && PanelId != CurrentPanel.PanelId)
	{
		return;
	}

	SubmitPanelButtonInternal(ButtonId, RowIndex);
}

void UTaskPanelChannelComponent::SubmitPanelButtonInternal(FName ButtonId, int32 RowIndex)
{
	if (!bPanelVisible || ButtonId.IsNone())
	{
		return;
	}

	APlayerController* PC = GetOwningController();
	if (!PC)
	{
		return;
	}

	OnPanelActionSubmitted.Broadcast(PC, CurrentPanel.PanelId, ButtonId, RowIndex);
}

void UTaskPanelChannelComponent::OnRep_PanelState()
{
	OnPanelStateChanged.Broadcast(CurrentPanel, bPanelVisible);
}

void UTaskPanelChannelComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UTaskPanelChannelComponent, CurrentPanel);
	DOREPLIFETIME(UTaskPanelChannelComponent, bPanelVisible);
}
