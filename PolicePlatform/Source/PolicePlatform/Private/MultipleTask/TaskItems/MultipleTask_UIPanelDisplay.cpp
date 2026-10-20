// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_UIPanelDisplay.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMultipleTask_UIPanelDisplay::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	UE_LOG(LogTemp, Log, TEXT("[UIPanelDisplay] Showing panel: Type=%d, Title=%s, AutoCloseDelay=%.1f"),
		(int32)PanelType, *PanelTitle.ToString(), AutoCloseDelay);

	// 启动自动关闭计时
	if (AutoCloseDelay > 0.f)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			World->GetTimerManager().SetTimer(AutoCloseTimerHandle, this, &UMultipleTask_UIPanelDisplay::EndTask,
				AutoCloseDelay, false);
		}
	}
}

void UMultipleTask_UIPanelDisplay::EndTask_Implementation()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(AutoCloseTimerHandle);
	}
	Super::EndTask_Implementation();
}
