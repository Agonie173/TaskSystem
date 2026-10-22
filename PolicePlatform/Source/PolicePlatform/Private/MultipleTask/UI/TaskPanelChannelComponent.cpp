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
		UE_LOG(LogTemp, Error, TEXT("[TaskPanelChannel] Failed to auto-create channel on PlayerState %s"), *PS->GetName());
		return nullptr;
	}
	NewChannel->RegisterComponent();
	UE_LOG(LogTemp, Log, TEXT("[TaskPanelChannel] Auto-created channel on PlayerState %s (owner PC %s)"),
		*PS->GetName(), *GetNameSafe(PC));
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

	UE_LOG(LogTemp, Log, TEXT("[TaskPanelChannel] ShowPanel '%s' -> %s"),
		*InPanel.PanelId.ToString(), *GetNameSafe(GetOwner()));

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
		/*UE_LOG(LogTemp, Verbose, TEXT("[TaskPanelChannel] HidePanel skipped: requested '%s' but current panel is '%s' (owner %s)"),
			*PanelId.ToString(), *CurrentPanel.PanelId.ToString(), *GetNameSafe(GetOwner()));*/
		return;
	}

	bPanelVisible = false;

	UE_LOG(LogTemp, Log, TEXT("[TaskPanelChannel] HidePanel '%s' -> %s"),
		*CurrentPanel.PanelId.ToString(), *GetNameSafe(GetOwner()));

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
			UE_LOG(LogTemp, Log, TEXT("[TaskPanelChannel] Client submits button '%s' row %d for panel '%s'"),
				*ButtonId.ToString(), RowIndex, *CurrentPanel.PanelId.ToString());
			Server_SubmitPanelButton(CurrentPanel.PanelId, ButtonId, RowIndex);
		}
		else
		{
			// 其他玩家 PlayerState 上的通道副本在客户端收到点击请求，直接忽略
			UE_LOG(LogTemp, Verbose, TEXT("[TaskPanelChannel] Non-local controller submit ignored (owner %s)"), *GetNameSafe(GetOwner()));
		}
		return;
	}

	SubmitPanelButtonInternal(ButtonId, RowIndex);
}

void UTaskPanelChannelComponent::SubmitCustomEvent(FName EventId, const FString& Payload)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		// 只有本地玩家自己的通道才能向服务器提交
		APlayerController* PC = GetOwningController();
		if (PC && PC->IsLocalController())
		{
			UE_LOG(LogTemp, Log, TEXT("[TaskPanelChannel] Client submits custom event '%s' (payload len=%d)"),
				*EventId.ToString(), Payload.Len());
			Server_SubmitCustomEvent(EventId, Payload);
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("[TaskPanelChannel] Non-local controller custom event ignored (owner %s)"), *GetNameSafe(GetOwner()));
		}
		return;
	}

	// 服务器本地直接广播
	SubmitCustomEventInternal(EventId, Payload);
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
		UE_LOG(LogTemp, Warning, TEXT("[TaskPanelChannel] Stale button click dropped: submitted panel '%s' but current is '%s' (owner %s)"),
			*PanelId.ToString(), *CurrentPanel.PanelId.ToString(), *GetNameSafe(GetOwner()));
		return;
	}

	SubmitPanelButtonInternal(ButtonId, RowIndex);
}

void UTaskPanelChannelComponent::Server_SubmitCustomEvent_Implementation(FName EventId, const FString& Payload)
{
	SubmitCustomEventInternal(EventId, Payload);
}

void UTaskPanelChannelComponent::SubmitCustomEventInternal(FName EventId, const FString& Payload)
{
	if (EventId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TaskPanelChannel] Custom event rejected: EventId is None (owner %s)"), *GetNameSafe(GetOwner()));
		return;
	}

	APlayerController* PC = GetOwningController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TaskPanelChannel] Custom event rejected: no owning PlayerController (owner %s)"),
			*GetNameSafe(GetOwner()));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[TaskPanelChannel] Custom event accepted: event '%s', PC %s, payload len=%d"),
		*EventId.ToString(), *GetNameSafe(PC), Payload.Len());

	OnCustomEventSubmitted.Broadcast(PC, EventId, Payload);
}

void UTaskPanelChannelComponent::SubmitPanelButtonInternal(FName ButtonId, int32 RowIndex)
{
	if (!bPanelVisible || ButtonId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TaskPanelChannel] Button submit rejected: panel visible=%d, ButtonId '%s' (owner %s)"),
			bPanelVisible ? 1 : 0, *ButtonId.ToString(), *GetNameSafe(GetOwner()));
		return;
	}

	APlayerController* PC = GetOwningController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TaskPanelChannel] Button submit rejected: no owning PlayerController (owner %s)"),
			*GetNameSafe(GetOwner()));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[TaskPanelChannel] Panel action accepted: panel '%s', button '%s', row %d, PC %s"),
		*CurrentPanel.PanelId.ToString(), *ButtonId.ToString(), RowIndex, *GetNameSafe(PC));

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
