// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/Actors/WidgetConfirmActor.h"

#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "MultipleTask/UI/TaskPanelChannelComponent.h"

AWidgetConfirmActor::AWidgetConfirmActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 纯客户端本地 Actor，服务器无对应实例
	bReplicates = false;

	// WidgetComponent：只创建组件，类/DrawSize/空间等全部在蓝图子类配置
	WidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComp"));
	WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WidgetComp->SetGenerateOverlapEvents(false);
	WidgetComp->SetVisibility(true);
}

void AWidgetConfirmActor::ReportConfirmed()
{
	// 仅客户端执行：服务器上没有这个 Actor 实例
	if (GetNetMode() != NM_Client)
	{
		return;
	}

	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!LocalPC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WidgetConfirmActor] ReportConfirmed: No local PlayerController"));
		return;
	}

	if (APlayerState* PS = LocalPC->PlayerState)
	{
		if (UTaskPanelChannelComponent* Channel = UTaskPanelChannelComponent::FindChannel(LocalPC))
		{
			const FString Payload = TaskInstanceId.IsNone()
				? GetFName().ToString()
				: TaskInstanceId.ToString();
			UE_LOG(LogTemp, Log, TEXT("[WidgetConfirmActor] Reporting confirm: TaskInstanceId=%s, PlayerId=%d"),
				*Payload, PS->GetPlayerId());
			Channel->SubmitCustomEvent(FName(TEXT("WidgetConfirm")), Payload);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[WidgetConfirmActor] ReportConfirmed: No TaskPanelChannelComponent on PlayerState"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[WidgetConfirmActor] ReportConfirmed: No PlayerState on local PC"));
	}
}

void AWidgetConfirmActor::HandleServerDestroy()
{
	UE_LOG(LogTemp, Log, TEXT("[WidgetConfirmActor] Server destroy received, TaskInstanceId=%s"),
		*TaskInstanceId.ToString());

	// 清掉 WidgetComponent 的 Widget（避免销毁时 UMG 报引用残留）
	if (WidgetComp)
	{
		WidgetComp->SetWidgetClass(nullptr);
		WidgetComp->SetWidget(nullptr);
	}

	Destroy();
}
