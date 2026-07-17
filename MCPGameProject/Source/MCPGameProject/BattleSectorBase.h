// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleSectorBase.generated.h"

class ABattleCampSector;
class ABattleDefenderCamp;
class ABattleSectorAnchor;

// Outcome of a battle session.
UENUM(BlueprintType)
enum class EBattleResult : uint8
{
	// Match still in progress.
	InProgress UMETA(DisplayName = "In Progress"),
	// Attackers captured every sector before time ran out.
	AttackersWin UMETA(DisplayName = "Attackers Win"),
	// Time ran out without attackers capturing all sectors.
	DefendersWin UMETA(DisplayName = "Defenders Win"),
};

// Top-level container actor that holds references to both camps and the sector(s),
// drives the match timer, and resolves win/lose conditions per wiki V1.
UCLASS()
class MCPGAMEPROJECT_API ABattleSectorBase : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABattleSectorBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Attacker camp reference (team 0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Camps")
	ABattleCampSector* AttackerCamp = nullptr;

	// Defender camp reference (team 1).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Camps")
	ABattleDefenderCamp* DefenderCamp = nullptr;

	// Sector anchor(s) that attackers must capture to win.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Sectors")
	TArray<ABattleSectorAnchor*> Sectors;

	// Total match duration in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Timing")
	float MatchDuration = 300.f;

	// Remaining match time in seconds (runtime-only, not edited/saved).
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Battle|Timing")
	float MatchTimeRemaining = 0.f;

	// Current match outcome.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Battle")
	EBattleResult Result = EBattleResult::InProgress;

	// True once the match has concluded (time ran out or all sectors captured); Tick stops evaluating after this.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Battle")
	bool bMatchOver = false;

	// Begin the match: resets the timer and activates win-condition evaluation.
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void StartMatch();

	// Stop the match and freeze evaluation.
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void StopMatch();

protected:
	// True if every sector in Sectors is currently owned by TeamId.
	bool OwningTeamHoldsAllSectors(int32 TeamId) const;

	// Evaluate the early-win condition (attackers captured everything). Returns true if the match has concluded.
	bool EvaluateWinCondition();
};
