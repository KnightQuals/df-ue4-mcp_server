// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BattleMapConfig.h"
#include "GameMode_Breakthrough.generated.h"

class ASpawnAreaHub;
class ABreakthroughCharacter;
class AForbiddenZone;
class ABattleAreaSpline;
class UDataTable;

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

	// ===== V2 map configuration (DataTable row key = MapId) =====
	// If not assigned in the Blueprint, BeginPlay loads /Game/Config/DT_BattleMapConfig.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|MapConfig")
	UDataTable* MapConfigTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|MapConfig")
	FName MapId = TEXT("NewMap");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "V2|MapConfig")
	FBattleMapConfig ActiveMapConfig;

	// Dynamically spawned red outer-border strips. The inner play space (bases, combat
	// area, and capture anchor) is safe; crossing a strip begins the V2 elimination timer.
	UPROPERTY(Transient)
	TArray<AForbiddenZone*> ForbiddenZones;

	// V2 closed spline representing the same safe battlefield area. Characters query
	// this polygon rather than relying on finite-width overlap strips.
	UPROPERTY(Transient)
	ABattleAreaSpline* SafeBattlefieldSpline = nullptr;

	// Display-only V2 regions: attacker/defender bases, active combat area, and
	// capture-sector outlines. Kept separate from the outer safe-boundary spline.
	UPROPERTY(Transient)
	TArray<ABattleAreaSpline*> GameplayAreaSplines;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Loads the MapId DataTable row and applies it to existing battle actors.
	void LoadAndApplyMapConfig();

	// Creates four V2 forbidden-zone border strips around the safe battlefield rectangle.
	void SpawnForbiddenZoneBorders();

	// Creates a closed, editable spline outline for the safe battlefield polygon.
	void SpawnSafeBattlefieldSpline();

	// Creates display-only color-coded splines for two bases, combat area, and sectors.
	void SpawnGameplayAreaSplines();

	// Called after a player joins the game.
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// Deferred AI-defender spawn: called after the first login so we can tell whether
	// a 2nd human player is joining before deciding to spawn the solo-demo AI.
	UFUNCTION()
	void TrySpawnDefenderAI();

	// A late second human player may join after the solo AI has spawned. Remove every
	// unpossessed Team-1 placeholder so packaged local multiplayer has exactly 2 pawns.
	void RemoveSoloDefenderAI();

	// Spawn the pawn for a new player, alternating attacker/defender team assignment.
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	// Number of pawns spawned so far, used to alternate team assignment (even = attacker, odd = defender).
	int32 SpawnedPawnCount = 0;

	// True after we spawned the defender AI dummy (V1 single-player simulation of attackers vs defenders).
	bool bDefenderAISpawned = false;

	// True once the deferred AI-spawn timer has been armed (so we only arm it once).
	bool bAISpawnTimerArmed = false;
};
