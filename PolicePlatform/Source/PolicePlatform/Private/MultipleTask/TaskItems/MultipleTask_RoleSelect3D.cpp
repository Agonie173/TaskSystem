// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_RoleSelect3D.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "MultipleTask/UI/TaskPanelChannelComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

const FName UMultipleTask_RoleSelect3D::EventSelect(TEXT("Role3D_Select"));
const FName UMultipleTask_RoleSelect3D::EventCancel(TEXT("Role3D_Cancel"));
const FName UMultipleTask_RoleSelect3D::EventLock(TEXT("Role3D_Lock"));
const FName UMultipleTask_RoleSelect3D::EventSpawn(TEXT("Role3D_Spawn"));
const FName UMultipleTask_RoleSelect3D::EventState(TEXT("Role3D_State"));
const FName UMultipleTask_RoleSelect3D::EventClose(TEXT("Role3D_Close"));

void UMultipleTask_RoleSelect3D::StartTask_Implementation()
{
	Super::StartTask_Implementation();

	// 配置校验
	if (RoleOptions.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RoleSelect3D] %s: RoleOptions is empty, task will wait forever"), *TaskName.ToString());
	}

	TSet<FName> SeenTags;
	for (const FRoleSelect3DOption& Option : RoleOptions)
	{
		if (Option.RoleTag.IsNone() || SeenTags.Contains(Option.RoleTag))
		{
			UE_LOG(LogTemp, Warning, TEXT("[RoleSelect3D] %s: invalid or duplicated RoleTag '%s'"),
				*TaskName.ToString(), *Option.RoleTag.ToString());
		}
		SeenTags.Add(Option.RoleTag);
	}

	// 下发生成指令 + 初始状态（BoardClass 未配置也照样下发，客户端用回退类生成）
	FVector Loc = BoardTransform.GetTranslation();
	const FRotator Rot = BoardTransform.Rotator();
	const FString SpawnPayload = FString::Printf(
		TEXT("{\"BoardClass\":\"%s\",\"WidgetClass\":\"%s\",\"X\":%.2f,\"Y\":%.2f,\"Z\":%.2f,\"Pitch\":%.2f,\"Yaw\":%.2f,\"Roll\":%.2f}"),
		*BoardActorClass.ToSoftObjectPath().ToString(),
		*BoardWidgetClass.ToSoftObjectPath().ToString(),
		Loc.X, Loc.Y, Loc.Z, Rot.Pitch, Rot.Yaw, Rot.Roll);
	BroadcastEvent(EventSpawn, SpawnPayload);
	BroadcastEvent(EventState, BuildStatePayload());

	UE_LOG(LogTemp, Log, TEXT("[RoleSelect3D] %s started: %d role options"), *TaskName.ToString(), RoleOptions.Num());
}

void UMultipleTask_RoleSelect3D::EndTask_Implementation()
{
	// 通知客户端销毁本地 UI
	BroadcastEvent(EventClose, TEXT("{}"));
	Occupancy.Reset();
	Super::EndTask_Implementation();
}

void UMultipleTask_RoleSelect3D::OnCustomEvent(APlayerController* PC, FName EventId, const FString& Payload)
{
	if (!PC || bTaskEnded)
	{
		return;
	}

	if (EventId == EventSelect)
	{
		HandleSelect(PC, Payload);
	}
	else if (EventId == EventCancel)
	{
		HandleCancel(PC, Payload);
	}
	else if (EventId == EventLock)
	{
		HandleLock(PC, Payload);
	}
}

// ==================== 上行处理 ====================

void UMultipleTask_RoleSelect3D::HandleSelect(APlayerController* PC, const FString& Payload)
{
	const FName RoleTag = ParseRoleTag(Payload);
	const bool bKnownTag = RoleOptions.ContainsByPredicate(
		[&RoleTag](const FRoleSelect3DOption& O) { return O.RoleTag == RoleTag; });
	if (RoleTag.IsNone() || !bKnownTag)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RoleSelect3D] Select rejected: unknown RoleTag '%s'"), *RoleTag.ToString());
		return;
	}

	// 已锁定的玩家不允许再操作
	if (IsPlayerLocked(PC))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[RoleSelect3D] Select rejected: player already locked"));
		return;
	}

	// 幂等：重复选同一身份直接忽略
	if (FRoleOccupancy* Existing = Occupancy.Find(RoleTag))
	{
		if (Existing->Player.Get() == PC)
		{
			return;
		}
	}

	// 目标身份被他人占用
	if (FRoleOccupancy* Existing = Occupancy.Find(RoleTag))
	{
		if (Existing->Player.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("[RoleSelect3D] Select rejected: '%s' occupied by %s"),
				*RoleTag.ToString(), *GetPlayerDisplayName(Existing->Player.Get()));
			return;
		}
	}

	// 一人同时只能占用一个身份
	for (const TPair<FName, FRoleOccupancy>& Pair : Occupancy)
	{
		if (Pair.Value.Player.Get() == PC)
		{
			UE_LOG(LogTemp, Log, TEXT("[RoleSelect3D] Select rejected: player already holds '%s' (cancel first)"),
				*Pair.Key.ToString());
			return;
		}
	}

	FRoleOccupancy& NewOccupancy = Occupancy.FindOrAdd(RoleTag);
	NewOccupancy.Player = PC;
	NewOccupancy.bLocked = false;

	UE_LOG(LogTemp, Log, TEXT("[RoleSelect3D] %s selected '%s'"), *GetPlayerDisplayName(PC), *RoleTag.ToString());
	BroadcastEvent(EventState, BuildStatePayload());
}

void UMultipleTask_RoleSelect3D::HandleCancel(APlayerController* PC, const FString& Payload)
{
	const FName RoleTag = ParseRoleTag(Payload);
	FRoleOccupancy* Existing = Occupancy.Find(RoleTag);
	if (!Existing || Existing->Player.Get() != PC)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[RoleSelect3D] Cancel rejected: not the occupant of '%s'"), *RoleTag.ToString());
		return;
	}

	if (Existing->bLocked)
	{
		UE_LOG(LogTemp, Log, TEXT("[RoleSelect3D] Cancel rejected: '%s' already locked"), *RoleTag.ToString());
		return;
	}

	Occupancy.Remove(RoleTag);
	UE_LOG(LogTemp, Log, TEXT("[RoleSelect3D] %s canceled '%s'"), *GetPlayerDisplayName(PC), *RoleTag.ToString());
	BroadcastEvent(EventState, BuildStatePayload());
}

void UMultipleTask_RoleSelect3D::HandleLock(APlayerController* PC, const FString& Payload)
{
	const FName RoleTag = ParseRoleTag(Payload);
	FRoleOccupancy* Existing = Occupancy.Find(RoleTag);
	if (!Existing || Existing->Player.Get() != PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RoleSelect3D] Lock rejected: not the occupant of '%s'"), *RoleTag.ToString());
		return;
	}

	if (Existing->bLocked)
	{
		return; // 幂等
	}

	Existing->bLocked = true;

	// 复用 Sequencer 身份分配：写入 RoleAssignments + 给 Pawn 贴 Tag
	if (OwnerSequencer)
	{
		OwnerSequencer->AssignRole(RoleTag, PC);
	}

	UE_LOG(LogTemp, Log, TEXT("[RoleSelect3D] %s locked '%s'"), *GetPlayerDisplayName(PC), *RoleTag.ToString());
	BroadcastEvent(EventState, BuildStatePayload());
	CheckAllLocked();
}

void UMultipleTask_RoleSelect3D::CheckAllLocked()
{
	if (!OwnerSequencer || bTaskEnded)
	{
		return;
	}

	const TArray<APlayerController*> Participants = OwnerSequencer->GetParticipants();
	if (Participants.Num() == 0)
	{
		return;
	}

	for (APlayerController* Participant : Participants)
	{
		if (!IsPlayerLocked(Participant))
		{
			return; // 还有人没锁定
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[RoleSelect3D] All %d participants locked, ending task"), Participants.Num());
	EndTask();
}

// ==================== 状态下发 ====================

void UMultipleTask_RoleSelect3D::BroadcastEvent(FName EventId, const FString& Payload) const
{
	if (!OwnerSequencer)
	{
		return;
	}

	for (APlayerController* Participant : OwnerSequencer->GetParticipants())
	{
		if (UTaskPanelChannelComponent* Channel = UTaskPanelChannelComponent::FindOrCreateChannel(Participant))
		{
			Channel->NotifyCustomEvent(EventId, Payload);
		}
	}
}

FString UMultipleTask_RoleSelect3D::BuildStatePayload() const
{
	FString RolesJson;
	for (const FRoleSelect3DOption& Option : RoleOptions)
	{
		const FRoleOccupancy* Found = Occupancy.Find(Option.RoleTag);

		FString State = TEXT("Free");
		FString By;
		if (Found && Found->bLocked)
		{
			State = TEXT("Locked");
			By = GetPlayerDisplayName(Found->Player.Get());
		}
		else if (Found && Found->Player.IsValid())
		{
			State = TEXT("Occupied");
			By = GetPlayerDisplayName(Found->Player.Get());
		}

		if (!RolesJson.IsEmpty())
		{
			RolesJson += TEXT(",");
		}
		RolesJson += FString::Printf(TEXT("{\"Tag\":\"%s\",\"Name\":\"%s\",\"State\":\"%s\",\"By\":\"%s\"}"),
			*Option.RoleTag.ToString(), *EscapeJson(Option.DisplayName.ToString()), *State, *EscapeJson(By));
	}
	return FString::Printf(TEXT("{\"Roles\":[%s]}"), *RolesJson);
}

// ==================== 工具 ====================

bool UMultipleTask_RoleSelect3D::IsRoleFree(FName RoleTag) const
{
	const FRoleOccupancy* Found = Occupancy.Find(RoleTag);
	return !Found || !Found->Player.IsValid();
}

bool UMultipleTask_RoleSelect3D::IsPlayerLocked(APlayerController* PC) const
{
	for (const TPair<FName, FRoleOccupancy>& Pair : Occupancy)
	{
		if (Pair.Value.Player.Get() == PC)
		{
			return Pair.Value.bLocked;
		}
	}
	return false;
}

FString UMultipleTask_RoleSelect3D::GetPlayerDisplayName(APlayerController* PC)
{
	if (!PC)
	{
		return TEXT("");
	}
	if (const APlayerState* PS = PC->PlayerState)
	{
		return PS->GetPlayerName();
	}
	return PC->GetName();
}

FString UMultipleTask_RoleSelect3D::EscapeJson(const FString& In)
{
	FString Out = In;
	Out.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Out.ReplaceInline(TEXT("\""), TEXT("\\\""));
	Out.ReplaceInline(TEXT("\n"), TEXT(""));
	Out.ReplaceInline(TEXT("\r"), TEXT(""));
	return Out;
}

FName UMultipleTask_RoleSelect3D::ParseRoleTag(const FString& Payload)
{
	FString Trimmed = Payload;
	Trimmed.TrimStartAndEndInline();
	if (Trimmed.Len() >= 2 && Trimmed.StartsWith(TEXT("\"")) && Trimmed.EndsWith(TEXT("\"")))
	{
		Trimmed = Trimmed.Mid(1, Trimmed.Len() - 2);
	}
	return FName(*Trimmed);
}
