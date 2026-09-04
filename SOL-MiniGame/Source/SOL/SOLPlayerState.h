// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "SOLPlayerState.generated.h"

// Per-player round result. Lives on the PlayerState (not the pawn) because a
// PlayerState survives pawn destruction and is replicated to *every* client —
// which is exactly what a scoreboard needs: after extracting (or dying) the
// pawn is gone but the result must stay visible to everyone.
UCLASS()
class SOL_API ASOLPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ASOLPlayerState();

	// Set once the player completes an extraction. Replicated to all clients
	// so the HUD can show "队友已撤离" and the settlement board stays honest.
	UPROPERTY(ReplicatedUsing = OnRep_bExtracted, VisibleAnywhere, Category = "SOL|Round")
	bool bExtracted = false;

	// Payout banked at extraction (carried value x zone multiplier).
	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Round")
	int32 ExtractedValue = 0;

	// How many item stacks were carried out (flavour for the settlement screen).
	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Round")
	int32 ExtractedStacks = 0;

	// Which zone was used, for the settlement line ("从 北部撤离点 撤离").
	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Round")
	FString ExtractedZoneName;

	// Set when the match timer runs out with this player still in the field.
	// The distinction matters: "撤离失败" means the loot was carried but never
	// banked, which is the whole risk half of a search-and-extract round.
	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Round")
	bool bFailedExtraction = false;

	// Value that was lost on a failed extraction (what the player was holding
	// when the clock ran out) — the number that makes the failure sting.
	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Round")
	int32 LostValue = 0;

	// ------------------------------------------------------------ combat log
	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Combat")
	int32 Kills = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Combat")
	int32 Deaths = 0;

	// Loot value the victims of this player were carrying. Kills are worth
	// what the other guy had on him, not a flat score — that is what makes
	// hunting a loaded player a real decision instead of a chore.
	UPROPERTY(Replicated, VisibleAnywhere, Category = "SOL|Combat")
	int32 KillBountyTotal = 0;

	// Bounty from the most recent kill, so the shooter's kill feed can quote
	// the number the moment it happens (read straight after the damage call).
	UPROPERTY(VisibleAnywhere, Category = "SOL|Combat")
	int32 LastKillBounty = 0;

	// Server-side settlement entry point, called by the extraction zone.
	void MarkExtracted(int32 Value, int32 Stacks, const FString& ZoneName);

	// Server-side: the match clock expired while this player was still out
	// there. Records what was lost so the settlement board can show it.
	void MarkExtractionFailed(int32 CarriedValue);

	// Server-side combat bookkeeping (called from the victim's death handler).
	void RegisterKill(int32 VictimCarriedValue);
	void RegisterDeath();

	// Server-side: wipe the round result so this player can play again. Must
	// run before RestartPlayer — PlayerCanRestart refuses anyone still flagged
	// as extracted or timed out.
	void ResetForNewRound();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_bExtracted();
};
