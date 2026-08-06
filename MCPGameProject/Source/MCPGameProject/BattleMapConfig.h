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

	// Capture progress gained per second for each-player advantage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Capture")
	float CaptureSpeed = 1.f;

	// ===== V2 Conquest scoring =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|Conquest")
	int32 ConquestScoreToWin = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|Conquest")
	float ConquestScoreInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|Conquest")
	int32 ConquestPointsPerOwnedSector = 1;
};
