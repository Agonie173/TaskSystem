 // Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_UIPanelDisplay.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMultipleTask_UIPanelDisplay::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	UE_LOG(LogTemp, Log, TEXT("[UIPanelDisplay] Showing panel: Type=%d, AutoCloseDelay=%.1f"),
		(int32)PanelType, AutoCloseDelay);

	// 受众摘要（面板已在基类 StartTask 中按受众下发）
	UE_LOG(LogTemp, Log, TEXT("[UIPanelDisplay] Panel audience: %d player(s)"), ResolvePanelAudience().Num());

	// 启动自动关闭计时（0 = 不自动关，跟随步骤生命周期）
	if (AutoCloseDelay > 0.f)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			World->GetTimerManager().SetTimer(AutoCloseTimerHandle, this, &UMultipleTask_UIPanelDisplay::EndTask,
				AutoCloseDelay, false);

			UE_LOG(LogTemp, Log, TEXT("[UIPanelDisplay] Auto close scheduled in %.1f s"), AutoCloseDelay);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[UIPanelDisplay] No world available, auto close disabled (type=%d)"), (int32)PanelType);
		}
	}
}

void UMultipleTask_UIPanelDisplay::EndTask_Implementation()
{
	UWorld* World = GetWorld();
	if (World)
	{
		// 定时器仍处于活跃状态，说明本次结束发生在自动关闭到期之前（外部手动结束）
		if (World->GetTimerManager().IsTimerActive(AutoCloseTimerHandle))
		{
			UE_LOG(LogTemp, Log, TEXT("[UIPanelDisplay] Manual end while auto close timer still pending (type=%d)"), (int32)PanelType);
		}

		World->GetTimerManager().ClearTimer(AutoCloseTimerHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("[UIPanelDisplay] Display panel task ended (type=%d)"), (int32)PanelType);

	Super::EndTask_Implementation();
}
