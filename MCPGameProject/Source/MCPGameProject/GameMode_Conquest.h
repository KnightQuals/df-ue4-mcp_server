// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameMode_Breakthrough.h"
#include "GameMode_Conquest.generated.h"

// V2 Conquest mode: a round-based "bell" contest. All sectors are active at once;
// every 60-second round awards one point per sector to its CURRENT owner, then starts
// the next round. After three rounds, the higher total wins (tie = draw).
UCLASS()
class MCPGAMEPROJECT_API AGameMode_Conquest : public AGameMode_Breakthrough
{
	GENERATED_BODY()

public:
	AGameMode_Conquest();

	// Read from FBattleMapConfig at BeginPlay. Exposed for quick PIE tuning.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|Conquest")
	float RoundSeconds = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|Conquest")
	int32 TotalRoundCount = 3;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	// Remaining time in the current 60-second round. The authoritative server owns
	// this clock and broadcasts only whole seconds through ABattleGameState.
	float RoundTimeRemaining = 60.f;
	int32 LastBroadcastSecond = -1;

	// Resolve the just-ended round: count current sector ownership and award exactly
	// one point per owned sector (two sectors therefore yield 2-0, 1-1, or 0-2).
	void ResolveCurrentRound();

	// Starts the next round like a fresh match: all human/AI characters return to
	// their team hubs and each anchor returns to defender ownership at -1 progress.
	void ResetBattlefieldForNextRound();
	void FinishConquestMatch(int32 WinnerTeam);
};
