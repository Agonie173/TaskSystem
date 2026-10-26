// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultipleTask/Actors/RoleSelectBoardActor.h"
#include "MultipleTask/UI/TaskPanelChannelComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "UObject/SoftObjectPath.h"

ARoleSelectBoard::ARoleSelectBoard()
{
	PrimaryActorTick.bCanEverTick = false;

	WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("Widget"));
	WidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	WidgetComponent->SetDrawSize(FVector2D(600.f, 800.f));
	RootComponent = WidgetComponent;
}

void ARoleSelectBoard::BeginPlay()
{
	Super::BeginPlay();
	BindChannel();
}

void ARoleSelectBoard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindChannel();
	Super::EndPlay(EndPlayReason);
}

// ==================== 生成载荷 ====================

namespace
{
	// 提取 "Key":数字 的数值（位置字段用）
	bool ExtractNumberValue(const FString& Json, const TCHAR* Key, double& OutValue)
	{
		const FString Pattern = FString::Printf(TEXT("\"%s\":"), Key);
		const int32 Pos = Json.Find(Pattern, ESearchCase::IgnoreCase);
		if (Pos == INDEX_NONE)
		{
			return false;
		}
		int32 Start = Pos + Pattern.Len();
		while (Start < Json.Len() && FChar::IsWhitespace(Json[Start]))
		{
			++Start;
		}
		int32 End = Start;
		while (End < Json.Len() &&
			(FChar::IsDigit(Json[End]) || Json[End] == TEXT('-') || Json[End] == TEXT('.') ||
			 Json[End] == TEXT('+') || Json[End] == TEXT('e') || Json[End] == TEXT('E')))
		{
			++End;
		}
		if (End <= Start)
		{
			return false;
		}
		OutValue = FCString::Atod(*Json.Mid(Start, End - Start));
		return true;
	}
}

bool ARoleSelectBoard::ApplyBoardPayload(const FString& SpawnPayloadJson)
{
	// 应用配表下发的世界位置和朝向（蓝图生成时可任意给初始位置，这里会纠正）
	double X = 0.0, Y = 0.0, Z = 0.0, Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
	if (ExtractNumberValue(SpawnPayloadJson, TEXT("X"), X) &&
		ExtractNumberValue(SpawnPayloadJson, TEXT("Y"), Y) &&
		ExtractNumberValue(SpawnPayloadJson, TEXT("Z"), Z))
	{
		ExtractNumberValue(SpawnPayloadJson, TEXT("Pitch"), Pitch);
		ExtractNumberValue(SpawnPayloadJson, TEXT("Yaw"), Yaw);
		ExtractNumberValue(SpawnPayloadJson, TEXT("Roll"), Roll);
		SetActorLocationAndRotation(FVector(X, Y, Z), FRotator(Pitch, Yaw, Roll));
	}

	// 极简解析：取 "WidgetClass":"..." 之间的类路径
	const FString Key = TEXT("\"WidgetClass\":\"");
	const int32 KeyPos = SpawnPayloadJson.Find(Key, ESearchCase::IgnoreCase);
	if (KeyPos == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RoleSelectBoard] Spawn payload missing WidgetClass: %s"), *SpawnPayloadJson);
		return false;
	}

	const int32 ValueStart = KeyPos + Key.Len();
	const int32 ValueEnd = SpawnPayloadJson.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
	if (ValueEnd == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RoleSelectBoard] Spawn payload malformed: %s"), *SpawnPayloadJson);
		return false;
	}

	const FString ClassPath = SpawnPayloadJson.Mid(ValueStart, ValueEnd - ValueStart);
	if (ClassPath.IsEmpty())
	{
		return false; // 配表未配 WBP，留给蓝图自行设置
	}

	UClass* LoadedClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr, *ClassPath);
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[RoleSelectBoard] Failed to load WidgetClass: %s"), *ClassPath);
		return false;
	}

	if (WidgetComponent)
	{
		WidgetComponent->SetWidgetClass(LoadedClass);
		UE_LOG(LogTemp, Log, TEXT("[RoleSelectBoard] WidgetClass applied: %s"), *ClassPath);
	}
	return true;
}

ARoleSelectBoard* ARoleSelectBoard::SpawnBoardFromPayload(UObject* WorldContextObject, const FString& SpawnPayloadJson, TSubclassOf<ARoleSelectBoard> FallbackClass)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	// 解析 "BoardClass"（配表下发的 Actor 类路径），失败则退回 FallbackClass
	UClass* BoardClass = FallbackClass ? FallbackClass.Get() : StaticClass();
	const FString Key = TEXT("\"BoardClass\":\"");
	const int32 KeyPos = SpawnPayloadJson.Find(Key, ESearchCase::IgnoreCase);
	if (KeyPos != INDEX_NONE)
	{
		const int32 ValueStart = KeyPos + Key.Len();
		const int32 ValueEnd = SpawnPayloadJson.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
		if (ValueEnd != INDEX_NONE)
		{
			const FString ClassPath = SpawnPayloadJson.Mid(ValueStart, ValueEnd - ValueStart);
			if (!ClassPath.IsEmpty())
			{
				if (UClass* Loaded = StaticLoadClass(AActor::StaticClass(), nullptr, *ClassPath))
				{
					BoardClass = Loaded;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[RoleSelectBoard] Failed to load BoardClass '%s', falling back to %s"),
						*ClassPath, *BoardClass->GetName());
				}
			}
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARoleSelectBoard* Board = World->SpawnActor<ARoleSelectBoard>(BoardClass, FTransform::Identity, Params);
	if (Board)
	{
		// 应用配表位置与 WBP 类
		Board->ApplyBoardPayload(SpawnPayloadJson);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[RoleSelectBoard] SpawnActor failed for class %s"), *BoardClass->GetName());
	}
	return Board;
}

// ==================== 状态解析 ====================

namespace
{
	// 从单个对象 JSON 片段提取 "Key":"Value" 的字符串值
	FString ExtractStringValue(const FString& Segment, const TCHAR* Key)
	{
		const FString Pattern = FString::Printf(TEXT("\"%s\":\""), Key);
		const int32 KeyPos = Segment.Find(Pattern, ESearchCase::CaseSensitive);
		if (KeyPos == INDEX_NONE)
		{
			return FString();
		}
		const int32 ValueStart = KeyPos + Pattern.Len();
		int32 ValueEnd = ValueStart;
		while (ValueEnd < Segment.Len())
		{
			if (Segment[ValueEnd] == TEXT('\\')) { ValueEnd += 2; continue; } // 跳过转义
			if (Segment[ValueEnd] == TEXT('"')) { break; }
			++ValueEnd;
		}
		FString Value = Segment.Mid(ValueStart, ValueEnd - ValueStart);
		Value.ReplaceInline(TEXT("\\\""), TEXT("\""));
		Value.ReplaceInline(TEXT("\\\\"), TEXT("\\"));
		return Value;
	}
}

TArray<FBoardRoleState> ARoleSelectBoard::ParseStatePayload(const FString& StatePayloadJson)
{
	TArray<FBoardRoleState> Result;

	// 定位 "Roles":[ ... ]
	const int32 ArrayStart = StatePayloadJson.Find(TEXT("\"Roles\":["));
	if (ArrayStart == INDEX_NONE)
	{
		return Result;
	}

	// 逐个对象切分：{...},{...}
	const int32 BodyStart = ArrayStart + FCString::Strlen(TEXT("\"Roles\":["));
	int32 Cursor = BodyStart;
	while (true)
	{
		const int32 ObjStart = StatePayloadJson.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
		if (ObjStart == INDEX_NONE)
		{
			break;
		}
		const int32 ObjEnd = StatePayloadJson.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ObjStart);
		if (ObjEnd == INDEX_NONE)
		{
			break;
		}

		const FString Segment = StatePayloadJson.Mid(ObjStart, ObjEnd - ObjStart + 1);
		FBoardRoleState Role;
		Role.Tag = FName(*ExtractStringValue(Segment, TEXT("Tag")));
		Role.DisplayName = ExtractStringValue(Segment, TEXT("Name"));
		Role.State = ExtractStringValue(Segment, TEXT("State"));
		Role.By = ExtractStringValue(Segment, TEXT("By"));
		if (!Role.Tag.IsNone())
		{
			Result.Add(MoveTemp(Role));
		}

		Cursor = ObjEnd + 1;
	}
	return Result;
}

// ==================== 上行请求 ====================
void ARoleSelectBoard::RequestSelectRole(FName RoleTag)
{
	if (BoundChannel)
	{
		BoundChannel->SubmitCustomEvent(TEXT("Role3D_Select"), RoleTag.ToString());
	}
}

void ARoleSelectBoard::RequestCancelRole(FName RoleTag)
{
	if (BoundChannel)
	{
		BoundChannel->SubmitCustomEvent(TEXT("Role3D_Cancel"), RoleTag.ToString());
	}
}

void ARoleSelectBoard::RequestLockRole(FName RoleTag)
{
	if (BoundChannel)
	{
		BoundChannel->SubmitCustomEvent(TEXT("Role3D_Lock"), RoleTag.ToString());
	}
}

// ==================== 通道绑定 ====================

void ARoleSelectBoard::BindChannel()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	if (APlayerState* PS = PC->PlayerState)
	{
		BoundChannel = PS->FindComponentByClass<UTaskPanelChannelComponent>();
		if (BoundChannel)
		{
			BoundChannel->OnCustomEventNotified.AddDynamic(this, &ARoleSelectBoard::HandleChannelEvent);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[RoleSelectBoard] No TaskPanelChannelComponent on local PlayerState"));
		}
	}
}

void ARoleSelectBoard::UnbindChannel()
{
	if (BoundChannel)
	{
		BoundChannel->OnCustomEventNotified.RemoveDynamic(this, &ARoleSelectBoard::HandleChannelEvent);
		BoundChannel = nullptr;
	}
}

void ARoleSelectBoard::HandleChannelEvent(FName EventId, const FString& Payload)
{
	// 只转发给 WBP，不做任何业务判断
	OnBoardEvent.Broadcast(EventId, Payload);
}
