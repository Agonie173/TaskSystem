// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/TaskItems/MultipleTask_ZoneMonitor.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ==================== 生命周期 ====================

void UMultipleTask_ZoneMonitor::StartTask_Implementation()
{
	ViolatingPawns.Reset();
	PenalizedPawns.Reset();

	if (ZoneActorTag.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ZoneMonitor] ZoneActorTag is not set, task ends immediately"));
		EndTask();
		return;
	}

	// 基类下发正常状态面板并绑定通道
	Super::StartTask_Implementation();

	InitFromCurrentOverlaps();
	RefreshComplianceTimer();

	UE_LOG(LogTemp, Log, TEXT("[ZoneMonitor] Monitoring zone %s (%s), %d player(s) already violating"),
		*ZoneActorTag.ToString(), bRequireInsideZone ? TEXT("must stay inside") : TEXT("must stay outside"),
		ViolatingPawns.Num());
}

void UMultipleTask_ZoneMonitor::EndTask_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ComplianceTimerHandle);
	}

	ViolatingPawns.Reset();

	Super::EndTask_Implementation();

	UE_LOG(LogTemp, Log, TEXT("[ZoneMonitor] Monitoring ended, %d violation(s) recorded"), PenalizedPawns.Num());
}

void UMultipleTask_ZoneMonitor::FinishMonitoring()
{
	UE_LOG(LogTemp, Log, TEXT("[ZoneMonitor] FinishMonitoring requested"));
	EndTask();
}

// ==================== 初始化 ====================

void UMultipleTask_ZoneMonitor::InitFromCurrentOverlaps()
{
	AActor* Zone = FindActorByTag(ZoneActorTag);
	if (!Zone)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ZoneMonitor] Zone actor with tag %s not found, skip initial check"),
			*ZoneActorTag.ToString());
		return;
	}

	TArray<AActor*> OverlappingActors;
	Zone->GetOverlappingActors(OverlappingActors, APawn::StaticClass());

	// 初始重叠扫描结果（仅日志，便于核对是否有人开局即违规）
	UE_LOG(LogTemp, Verbose, TEXT("[ZoneMonitor] Initial overlap scan found %d pawn(s) in zone"),
		OverlappingActors.Num());

	// 只按面板受众里的参训玩家初始化，避免把观战端算进来
	// 口径注意：这里按面板受众过滤，与事件路径（全部参训者）不一致，见类注释的已知口径差异
	for (APlayerController* PC : ResolvePanelAudience())
	{
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}

		const bool bInside = OverlappingActors.Contains(Pawn);
		const bool bViolating = bRequireInsideZone ? !bInside : bInside;
		if (bViolating)
		{
			SetPlayerViolating(Pawn, true);
		}
	}
}

// ==================== 事件 ====================

void UMultipleTask_ZoneMonitor::OnActorOverlapBegin(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!OverlappedActor || !OtherActor)
	{
		return;
	}

	if (!ZoneActorTag.IsNone() && !OverlappedActor->ActorHasTag(ZoneActorTag))
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!IsWatchedPawn(Pawn))
	{
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[ZoneMonitor] OverlapBegin on %s by %s"),
		*OverlappedActor->GetName(), *Pawn->GetName());

	// 进入区域：要求待在里面时合规，要求待在外面时违规
	SetPlayerViolating(Pawn, !bRequireInsideZone);
}

void UMultipleTask_ZoneMonitor::OnActorOverlapEnd(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!OverlappedActor || !OtherActor)
	{
		return;
	}

	if (!ZoneActorTag.IsNone() && !OverlappedActor->ActorHasTag(ZoneActorTag))
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!IsWatchedPawn(Pawn))
	{
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[ZoneMonitor] OverlapEnd on %s by %s"),
		*OverlappedActor->GetName(), *Pawn->GetName());

	// 区域 Actor 可能有多个碰撞组件：离开其中一个不代表真的离开区域，
	// 用实际重叠结果复核，避免误报违规
	if (AActor* Zone = FindActorByTag(ZoneActorTag))
	{
		TArray<AActor*> OverlappingActors;
		Zone->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
		if (OverlappingActors.Contains(Pawn))
		{
			UE_LOG(LogTemp, Verbose, TEXT("[ZoneMonitor] %s still overlaps zone via other component, ignore"),
				*Pawn->GetName());
			return;
		}
	}

	// 离开区域：要求待在里面时违规，要求待在外面时合规
	SetPlayerViolating(Pawn, bRequireInsideZone);
}

// ==================== 状态 ====================

void UMultipleTask_ZoneMonitor::SetPlayerViolating(APawn* PlayerPawn, bool bViolating)
{
	const bool bAlreadyViolating = ViolatingPawns.Contains(PlayerPawn);
	if (bViolating == bAlreadyViolating)
	{
		return;
	}

	if (bViolating)
	{
		ViolatingPawns.Add(PlayerPawn);
		UE_LOG(LogTemp, Warning, TEXT("[ZoneMonitor] %s violated zone %s"),
			*PlayerPawn->GetName(), *ZoneActorTag.ToString());

		// 同一玩家只记一次分，反复进出不重复扣
		if (!PenalizedPawns.Contains(PlayerPawn))
		{
			PenalizedPawns.Add(PlayerPawn);
			ReportViolation(PlayerPawn);
		}
	}
	else
	{
		ViolatingPawns.Remove(PlayerPawn);
		UE_LOG(LogTemp, Log, TEXT("[ZoneMonitor] %s back to compliant state"), *PlayerPawn->GetName());
	}

	UpdatePlayerPanel(PlayerPawn);
	RefreshComplianceTimer();
}

void UMultipleTask_ZoneMonitor::ReportViolation(APawn* PlayerPawn)
{
	// 评分按身份记录，未分配身份时只能记日志
	const FName RoleTag = OwnerSequencer ? OwnerSequencer->GetRoleTagByPawn(PlayerPawn) : NAME_None;
	if (RoleTag.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ZoneMonitor] %s has no role tag, violation not scored"),
			*PlayerPawn->GetName());
		return;
	}

	ReportScoreEvent(RoleTag, ViolationErrorCode);

	UE_LOG(LogTemp, Log, TEXT("[ZoneMonitor] Violation reported: role %s, code %s"),
		*RoleTag.ToString(), *ViolationErrorCode.ToString());
}

// ==================== 面板 ====================

FTaskPanelData UMultipleTask_ZoneMonitor::BuildWarningPanelData() const
{
	FTaskPanelData Data = BuildPanelData();
	Data.PanelType = WarningPanelType;
	// WarningTitle / WarningContent 不再写入面板数据（Title/Content 字段已移除）；
	// 外层 UI 根据 WarningPanelType 自行渲染警告文本
	return Data;
}

void UMultipleTask_ZoneMonitor::UpdatePlayerPanel(APawn* PlayerPawn)
{
	APlayerController* PC = GetControllerForPawn(PlayerPawn);
	if (!PC)
	{
		return;
	}

	if (ViolatingPawns.Contains(PlayerPawn))
	{
		PushPanelDataToController(PC, BuildWarningPanelData());
	}
	else
	{
		PushPanelDataToController(PC, BuildPanelData());
	}
}

// ==================== 计时 ====================

void UMultipleTask_ZoneMonitor::RefreshComplianceTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(ComplianceTimerHandle);

	if (RequiredCompliantDuration <= 0.f || ViolatingPawns.Num() > 0)
	{
		return;
	}

	World->GetTimerManager().SetTimer(ComplianceTimerHandle, this,
		&UMultipleTask_ZoneMonitor::HandleCompliantDurationReached, RequiredCompliantDuration, false);

	UE_LOG(LogTemp, Verbose, TEXT("[ZoneMonitor] Compliance timer armed for %.1fs"), RequiredCompliantDuration);
}

void UMultipleTask_ZoneMonitor::HandleCompliantDurationReached()
{
	UE_LOG(LogTemp, Log, TEXT("[ZoneMonitor] All players compliant for %.1fs, task ending"),
		RequiredCompliantDuration);
	EndTask();
}

// ==================== 查询 ====================

bool UMultipleTask_ZoneMonitor::IsPlayerViolating(APawn* PlayerPawn) const
{
	return PlayerPawn && ViolatingPawns.Contains(PlayerPawn);
}

bool UMultipleTask_ZoneMonitor::HasPlayerBeenPenalized(APawn* PlayerPawn) const
{
	return PlayerPawn && PenalizedPawns.Contains(PlayerPawn);
}

APlayerController* UMultipleTask_ZoneMonitor::GetControllerForPawn(APawn* PlayerPawn) const
{
	return PlayerPawn ? Cast<APlayerController>(PlayerPawn->GetController()) : nullptr;
}

bool UMultipleTask_ZoneMonitor::IsWatchedPawn(APawn* PlayerPawn) const
{
	if (!PlayerPawn)
	{
		return false;
	}

	APlayerController* PC = GetControllerForPawn(PlayerPawn);
	if (!PC)
	{
		return false;
	}

	// 单机调试兜底：没有注册流程时不额外过滤
	if (!OwnerSequencer || OwnerSequencer->GetParticipants().Num() == 0)
	{
		return true;
	}

	return OwnerSequencer->IsParticipant(PC);
}
