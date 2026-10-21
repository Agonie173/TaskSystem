// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/UI/TaskWorldWidgetComponent.h"
#include "Net/UnrealNetwork.h"

UTaskWorldWidgetComponent::UTaskWorldWidgetComponent()
{
	// 组件级复制开关：随宿主 Pawn 复制到客户端
	SetIsReplicatedByDefault(true);
}

void UTaskWorldWidgetComponent::SetReplicatedWidgetClass(TSubclassOf<UUserWidget> InWidgetClass)
{
	ReplicatedWidgetClass = InWidgetClass;
	// 服务器本地立即生效（服务器自己也要显示）
	SetWidgetClass(InWidgetClass);
}

void UTaskWorldWidgetComponent::SetReplicatedDrawConfig(FVector2D InDrawSize, bool bInDrawAtDesiredSize)
{
	ReplicatedDrawSize = InDrawSize;
	bReplicatedDrawAtDesiredSize = bInDrawAtDesiredSize;
	// 服务器本地立即生效（调用时机在 RegisterComponent 之前，渲染状态按这些值创建）
	SetDrawSize(InDrawSize);
	bDrawAtDesiredSize = bInDrawAtDesiredSize;
}

void UTaskWorldWidgetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UTaskWorldWidgetComponent, ReplicatedWidgetClass);
	DOREPLIFETIME(UTaskWorldWidgetComponent, ReplicatedDrawSize);
	DOREPLIFETIME(UTaskWorldWidgetComponent, bReplicatedDrawAtDesiredSize);
}

void UTaskWorldWidgetComponent::OnRep_ReplicatedWidgetClass()
{
	if (ReplicatedWidgetClass)
	{
		SetWidgetClass(ReplicatedWidgetClass);
		// 复制创建的组件不会自动走 InitWidget，这里手动重建 Widget 实例
		InitWidget();
	}
}

void UTaskWorldWidgetComponent::OnRep_ReplicatedDrawConfig()
{
	SetDrawSize(ReplicatedDrawSize);
	bDrawAtDesiredSize = bReplicatedDrawAtDesiredSize;
}
