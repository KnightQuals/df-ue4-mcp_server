// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameMode_Breakthrough.h"
#include "SpawnAreaHub.h"
#include "BreakthroughCharacter.h"
#include "DefaultPlayerController.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"

AGameMode_Breakthrough::AGameMode_Breakthrough()
{
	DefaultPawnClass = ABreakthroughCharacter::StaticClass();
	PlayerControllerClass = ADefaultPlayerController::StaticClass();
}

void AGameMode_Breakthrough::BeginPlay()
{
	Super::BeginPlay();

	// Find the level's spawn hub.
	for (TActorIterator<ASpawnAreaHub> It(GetWorld()); It; ++It)
	{
		SpawnHub = *It;
		break;
	}

	UE_LOG(LogTemp, Warning, TEXT("GameMode_Breakthrough started, SpawnHub=%s"),
		SpawnHub ? *SpawnHub->GetName() : TEXT("None"));
}

APawn* AGameMode_Breakthrough::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	if (!NewPlayer)
	{
		return nullptr;
	}

	// Alternate team assignment: even spawn index = attacker (team 0), odd = defender (team 1).
	const int32 Team = SpawnedPawnCount % 2;
	SpawnedPawnCount++;

	TSubclassOf<ABreakthroughCharacter> PawnClass = (Team == 0) ? AttackerClass : DefenderClass;
	if (!PawnClass)
	{
		PawnClass = ABreakthroughCharacter::StaticClass();
	}

	// Pick a spawn transform: prefer a random point from the spawn hub, fall back to StartSpot.
	FVector SpawnLocation = StartSpot ? StartSpot->GetActorLocation() : FVector::ZeroVector;
	FRotator SpawnRotation = StartSpot ? StartSpot->GetActorRotation() : FRotator::ZeroRotator;

	if (SpawnHub)
	{
		if (AActor* SpawnPoint = SpawnHub->GetRandomSpawnPoint())
		{
			SpawnLocation = SpawnPoint->GetActorLocation();
			SpawnRotation = SpawnPoint->GetActorRotation();
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = NewPlayer;

	ABreakthroughCharacter* NewPawn = GetWorld()->SpawnActor<ABreakthroughCharacter>(PawnClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (NewPawn)
	{
		NewPawn->Team = Team;
		UE_LOG(LogTemp, Warning, TEXT("SpawnDefaultPawnFor: spawned team=%d at %s"), Team, *SpawnLocation.ToString());
	}

	return NewPawn;
}
