#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Settings/Data.h"
#include "PPGameModeBase.generated.h"

class UWSSubsystem;
class APPPlayerControllerBase;


/**
 * 游戏模式基础类
 */
UCLASS()
class POLICEPLATFORM_API APPGameModeBase : public AGameMode
{
	GENERATED_BODY()
	
public:
	APPGameModeBase();

	UPROPERTY()
	UWSSubsystem* GWSSubsystem = nullptr;
	
	//学员SN 控制器字典
	UPROPERTY(BlueprintReadOnly, Category = "警务|GameMode")
	TMap<FString, APPPlayerControllerBase*> GSN_PCMap;
	
	
	virtual void BeginPlay() override;
	virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
};