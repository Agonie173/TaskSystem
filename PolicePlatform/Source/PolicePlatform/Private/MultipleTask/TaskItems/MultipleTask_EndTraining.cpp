// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_EndTraining.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "GameFramework/PlayerController.h"

UMultipleTask_EndTraining::UMultipleTask_EndTraining()
{
	PanelType = EUIPanelType::EventInfo;
	PanelTitle = FText::FromString(TEXT("本次训练完成"));
	PanelContent = FText::FromString(TEXT("持械对峙处置、结果上报、人员伤情排查和装备清点均已完成。"));

	ReturnToListButtonText = FText::FromString(TEXT("返回课件列表"));
	RestartButtonText = FText::FromString(TEXT("重新训练本课件"));
}

FTaskPanelData UMultipleTask_EndTraining::BuildPanelData() const
{
	FTaskPanelData Data = Super::BuildPanelData();

	FTaskPanelButton ReturnButton;
	ReturnButton.ButtonId = ReturnToListButtonId;
	ReturnButton.Label = ReturnToListButtonText;
	Data.Buttons.Add(ReturnButton);

	FTaskPanelButton RestartButton;
	RestartButton.ButtonId = RestartButtonId;
	RestartButton.Label = RestartButtonText;
	Data.Buttons.Add(RestartButton);

	return Data;
}

void UMultipleTask_EndTraining::HandlePanelAction_Implementation(APlayerController* PC, FName ButtonId, int32 RowIndex)
{
	if (ButtonId != ReturnToListButtonId && ButtonId != RestartButtonId)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[EndTraining] %s chose %s"),
		PC ? *PC->GetName() : TEXT("Unknown"), *ButtonId.ToString());

	// 任务系统不切场景/不重开，只把选择抛给外层框架
	if (OwnerSequencer)
	{
		OwnerSequencer->NotifyChoiceMade(ButtonId, PC);
	}

	EndTask();
}
