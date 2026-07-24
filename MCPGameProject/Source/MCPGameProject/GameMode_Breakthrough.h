// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameMode_Breakthrough.generated.h"

class ASpawnAreaHub;
class ABreakthroughCharacter;

// V1 game mode for the Breakthrough sector battle: finds the level's spawn hub and
// spawns players as either attackers or defenders, alternating team assignment.
UCLASS()
class MCPGAMEPROJECT_API AGameMode_Breakthrough : public AGameModeBase
{
	GENERATED_BODY()

public:
	// Sets default values for this game mode's properties
	AGameMode_Breakthrough();

	// Spawn hub used to pick a random spawn point for attacker-team players (team 0).
	UPROPERTY()
	ASpawnAreaHub* AttackerHub;

	// Spawn hub used to pick a random spawn point for defender-team players (team 1).
	UPROPERTY()
	ASpawnAreaHub* DefenderHub;

	// Pawn class spawned for attacker-team players (team 0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	TSubclassOf<ABreakthroughCharacter> AttackerClass;

	// Pawn class spawned for defender-team players (team 1).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	TSubclassOf<ABreakthroughCharacter> DefenderClass;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called after a player joins the game.
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// Spawn the pawn for a new player, alternating attacker/defender team assignment.
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	// Number of pawns spawned so far, used to alternate team assignment (even = attacker, odd = defender).
	int32 SpawnedPawnCount = 0;

	// True after we spawned the defender AI dummy (V1 single-player simulation of attackers vs defenders).
	bool bDefenderAISpawned = false;
};
