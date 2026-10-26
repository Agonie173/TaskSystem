// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MultipleTask/TaskItems/MultipleTaskBase.h"
#include "MultipleTask_RoleSelect3D.generated.h"

class APlayerController;
class UTaskPanelChannelComponent;

/** 单个身份选项配置 */
USTRUCT(BlueprintType)
struct FRoleSelect3DOption
{
	GENERATED_BODY()

	/** 身份 Tag（锁定后贴到玩家 Pawn 上） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName RoleTag;

	/** 显示名（下发到 UI 展示） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText DisplayName;
};

/**
 * 3D 选择板身份选择任务：服务器权威管理"选项占用/锁定"状态，
 * 通过 TaskPanelChannelComponent 双向自定义事件与每个客户端本地生成的 3D UI Actor 通信。
 *
 * 事件协议（EventId 约定，Payload 为 JSON 文本）：
 *   下行  Role3D_Spawn  {"BoardClass":"Actor类路径","WidgetClass":"WBP类路径","X":..,"Y":..,"Z":..,"Yaw":..,"Pitch":..,"Roll":..}
 *   下行  Role3D_State  {"Roles":[{"Tag":"..","Name":"..","State":"Free|Occupied|Locked","By":"玩家名"}]}
 *   下行  Role3D_Close  {}
 *   上行  Role3D_Select / Role3D_Cancel / Role3D_Lock   "RoleTag"
 *
 * 规则：
 *   - 一人同时只能占用一个身份；被占用选项他人不可选；取消后释放
 *   - 锁定需已占用；锁定后不可更改且给玩家 Pawn 贴身份 Tag（复用 Sequencer AssignRole）
 *   - 所有已注册参训玩家都锁定后任务结束；断线玩家保持占用
 */
UCLASS(CollapseCategories)
class POLICEPLATFORM_API UMultipleTask_RoleSelect3D : public UMultipleTaskBase
{
	GENERATED_BODY()

public:
	virtual void StartTask_Implementation() override;
	virtual void EndTask_Implementation() override;
	virtual void OnCustomEvent(APlayerController* PC, FName EventId, const FString& Payload) override;

	// ==================== 配置 ====================

	/** 身份选项列表（Tag 不可重复） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RoleSelect3D")
	TArray<FRoleSelect3DOption> RoleOptions;

	/** 客户端本地生成的 3D UI Actor 类（蓝图子类或 C++ 类），随 Spawn 事件下发路径；未配置时客户端使用回退类 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RoleSelect3D")
	TSoftClassPtr<AActor> BoardActorClass;

	/** Board 上 WidgetComponent 显示的 WBP 类，随 Spawn 事件下发；未配置时需蓝图自行 SetWidgetClass */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RoleSelect3D")
	TSoftClassPtr<UUserWidget> BoardWidgetClass;

	/** 3D UI 生成位置（固定世界坐标，所有客户端相同） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RoleSelect3D")
	FTransform BoardTransform;

	// ==================== 查询（调试/外部用） ====================

	/** 该身份当前是否空闲 */
	UFUNCTION(BlueprintPure, Category = "RoleSelect3D")
	bool IsRoleFree(FName RoleTag) const;

	/** 该玩家是否已锁定 */
	UFUNCTION(BlueprintPure, Category = "RoleSelect3D")
	bool IsPlayerLocked(APlayerController* PC) const;

private:
	/** 单个身份的占用状态 */
	struct FRoleOccupancy
	{
		TWeakObjectPtr<APlayerController> Player;
		bool bLocked = false;
	};

	/** 身份 → 占用状态（服务器权威） */
	TMap<FName, FRoleOccupancy> Occupancy;

	// 上行事件 Id
	static const FName EventSelect;
	static const FName EventCancel;
	static const FName EventLock;
	// 下行事件 Id
	static const FName EventSpawn;
	static const FName EventState;
	static const FName EventClose;

	void HandleSelect(APlayerController* PC, const FString& Payload);
	void HandleCancel(APlayerController* PC, const FString& Payload);
	void HandleLock(APlayerController* PC, const FString& Payload);
	void CheckAllLocked();

	// 对所有参训玩家下发事件
	void BroadcastEvent(FName EventId, const FString& Payload) const;
	// 组装全量状态 JSON
	FString BuildStatePayload() const;
	// 玩家显示名
	static FString GetPlayerDisplayName(APlayerController* PC);
	// 简单 JSON 字符串转义
	static FString EscapeJson(const FString& In);
	// 从 payload 提取 RoleTag（整个 payload 即 Tag 文本，去空白）
	static FName ParseRoleTag(const FString& Payload);
};
