// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BattleGameState.generated.h"

// Shared replicated match state used by V2 Conquest. Both player windows receive
// the same scores and score-limit result without relying on client-side simulation.
UCLASS()
class MCPGAMEPROJECT_API ABattleGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ABattleGameState();

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Scores, Category = "V2|Conquest")
	int32 AttackerScore = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Scores, Category = "V2|Conquest")
	int32 DefenderScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|Conquest")
	int32 ScoreToWin = 100;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "V2|Conquest")
	bool bConquestOver = false;

	// Match countdown in whole seconds, server-broadcast once per second. The HUD
	// renders this; clients never simulate their own clock. In round-based Conquest
	// this counts down the CURRENT round.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "V2|Conquest")
	int32 RemainingMatchSeconds = 0;

	// -1 = match running, 0 = attackers win, 1 = defenders win, 2 = draw (tied totals).
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "V2|Conquest")
	int32 ConquestWinner = -1;

	// Round-based "bell" Conquest (user redesign 2026-08-13): 1-based current round,
	// total rounds, and each team's gain in the most recently resolved round (for
	// the HUD round-result banner).
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "V2|Conquest")
	int32 CurrentRound = 1;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "V2|Conquest")
	int32 TotalRounds = 3;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "V2|Conquest")
	int32 LastRoundAtkGain = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "V2|Conquest")
	int32 LastRoundDefGain = 0;

	// Authority-only score mutation. Team 0=attackers, Team 1=defenders.
	void AddScore(int32 Team, int32 Amount);

	UFUNCTION()
	void OnRep_Scores();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};
