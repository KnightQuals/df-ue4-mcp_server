// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BattleHUD.generated.h"

class ABattleGameState;
class ABattleSectorAnchor;

// V2 match HUD (user request 2026-08-12): replaces one-shot on-screen debug text
// with a persistent, canvas-drawn interface.
//  - Top center: Conquest scoreboard (attackers red / defenders blue) + countdown.
//  - Center: winner banner when the match ends.
//  - Bottom center: tug-of-war capture progress bar while the local player stands
//    inside an active sector's capture radius.
// All data comes from replicated properties (GameState + anchors), so every player
// window renders the same state without client-side simulation.
UCLASS()
class MCPGAMEPROJECT_API ABattleHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawConquestPanel(ABattleGameState* State);
	void DrawResultBanner(ABattleGameState* State);
	void DrawCaptureBar();

	// Returns the active anchor whose capture radius contains the local pawn (nearest wins).
	ABattleSectorAnchor* FindRelevantAnchor() const;

	static const FLinearColor AttackerColor;
	static const FLinearColor DefenderColor;
};
