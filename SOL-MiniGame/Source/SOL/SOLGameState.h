// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "SOLGameState.generated.h"

// Round-level state shared by everyone. Two jobs:
//  1) count how many players have extracted, so the HUD can say "2/3 已撤离"
//     without every client walking the player array itself;
//  2) own the round-over decision (everyone is out), which is the only piece
//     of game flow that no single player's pawn can decide.
//
// It lives on the GameState rather than the GameMode because GameMode exists
// only on the server: a client HUD cannot read it. GameState is the server's
// public bulletin board.
UCLASS()
class SOL_API ASOLGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ASOLGameState();

	// Replicated round summary, recomputed on the server whenever a player
	// extracts (not every tick — this is event-driven state).
	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Round")
	int32 ExtractedCount = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Round")
	int32 TotalPlayers = 0;

	// Set once every connected player has extracted. Clients use it to switch
	// the settlement board headline from "你已撤离" to "本局结束".
	UPROPERTY(ReplicatedUsing = OnRep_bRoundOver, VisibleAnywhere, Category = "SOL|Round")
	bool bRoundOver = false;

	// Total value banked by everyone this round (a team-effort readout).
	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Round")
	int32 TeamValue = 0;

	// ----------------------------------------------------------- match clock
	// Seconds left in the round. Stored and replicated as an integer on
	// purpose: a float ticking every frame would spend bandwidth on
	// sub-second noise nobody can read, so the server only ever pushes whole
	// seconds (same trick as the battlefield countdown).
	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Round")
	int32 RemainingSeconds = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Round")
	int32 MatchDurationSeconds = 0;

	// Server-side: seed the clock at match start.
	void StartMatchClock(float TotalSeconds);

	// Server-side: subtract one tick. Returns true while the round is still
	// running, false on the tick that reaches zero.
	bool AdvanceMatchClock(int32 DeltaSeconds);

	// Server-side: force the round closed (timeout path). Extraction-driven
	// completion goes through RefreshRoundState instead.
	void FinishRound();

	// Server-side: clear the round-over state for a restart in place.
	void ResetRoundState();

	// True when the round ended because the clock ran out rather than because
	// everybody got out — the settlement headline differs.
	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Round")
	bool bTimedOut = false;

	// Server-side: recount from the player array and update the flags.
	// Called by the extraction zone right after a settlement.
	void RefreshRoundState();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_bRoundOver();
};
