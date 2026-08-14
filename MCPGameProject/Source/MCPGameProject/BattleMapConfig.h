// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BattleMapConfig.generated.h"

// One DataTable row per playable map. The row key is MapId (for example NewMap).
// V2 moves match/forbidden-zone tuning out of hard-coded gameplay classes.
USTRUCT(BlueprintType)
struct MCPGAMEPROJECT_API FBattleMapConfig : public FTableRowBase
{
	GENERATED_BODY()

	// Total time attackers have to capture every sector.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Timing")
	float MatchDuration = 300.f;

	// Seconds a player may remain in the outer forbidden zone before elimination.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|ForbiddenZone")
	float ForbiddenCountdown = 10.f;

	// Seconds after forbidden-zone elimination before the player is respawned at their base.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|ForbiddenZone")
	float RespawnDelay = 3.f;

	// Safe playable rectangle half extent on X (cm). Outside it is forbidden.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|ForbiddenZone")
	float SafeHalfExtentX = 4500.f;

	// Safe playable rectangle half extent on Y (cm). Outside it is forbidden.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|ForbiddenZone")
	float SafeHalfExtentY = 3600.f;

	// Width of each physical forbidden-trigger border strip (cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|ForbiddenZone")
	float ForbiddenBorderWidth = 1200.f;

	// Capture progress gained per second of advantage. Progress spans [-1, 1], so
	// 0.4 means a full flip takes ~5 seconds (faster demo pacing, user request 2026-08-14).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Capture")
	float CaptureSpeed = 0.4f;

	// ===== V2 Conquest scoring =====
	// Round-based "bell" scoring (user redesign 2026-08-13): each round lasts
	// ConquestRoundSeconds; when the timer ends, every sector awards 1 point to its
	// CURRENT owner; after ConquestRounds rounds the higher total wins, tie = draw.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|Conquest")
	int32 ConquestRounds = 3;

	// 30s rounds for the demo build (user request 2026-08-14; was 60s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|Conquest")
	float ConquestRoundSeconds = 30.f;

	// Legacy continuous-trickle parameters, unused by round-based scoring.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|Conquest")
	int32 ConquestScoreToWin = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|Conquest")
	float ConquestScoreInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|Conquest")
	int32 ConquestPointsPerOwnedSector = 1;
};
