// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/SuspectPawn.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "MultipleTask/TaskSequencerComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

ASuspectPawn::ASuspectPawn()
{
	bReplicates = true;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootScene);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootScene);
	TriggerBox->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetHiddenInGame(true);

	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	AudioComponent->SetupAttachment(RootScene);
	AudioComponent->bAutoActivate = false;
}

void ASuspectPawn::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ASuspectPawn::HandleTriggerBegin);
		TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ASuspectPawn::HandleTriggerEnd);
		UE_LOG(LogTemp, Verbose, TEXT("[SuspectPawn] Overlap delegates bound on %s"), *GetName());
	}
}

UTaskSequencerComponent* ASuspectPawn::GetSequencer() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<UTaskSequencerComponent>() : nullptr;
}

void ASuspectPawn::HandleTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 只在服务器广播，避免客户端重复转发
	if (GetLocalRole() != ROLE_Authority || !OtherActor)
	{
		return;
	}

	if (UTaskSequencerComponent* Sequencer = GetSequencer())
	{
		UE_LOG(LogTemp, Log, TEXT("[SuspectPawn] ForwardOverlapBegin: suspect %s, entering actor %s"),
			*GetName(), *GetNameSafe(OtherActor));
		Sequencer->ForwardOverlapBegin(this, OtherActor);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("[SuspectPawn] No sequencer on GameState, begin overlap from %s dropped"),
			*GetNameSafe(OtherActor));
	}
}

void ASuspectPawn::HandleTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (GetLocalRole() != ROLE_Authority || !OtherActor)
	{
		return;
	}

	if (UTaskSequencerComponent* Sequencer = GetSequencer())
	{
		UE_LOG(LogTemp, Log, TEXT("[SuspectPawn] ForwardOverlapEnd: suspect %s, leaving actor %s"),
			*GetName(), *GetNameSafe(OtherActor));
		Sequencer->ForwardOverlapEnd(this, OtherActor);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("[SuspectPawn] No sequencer on GameState, end overlap from %s dropped"),
			*GetNameSafe(OtherActor));
	}
}
