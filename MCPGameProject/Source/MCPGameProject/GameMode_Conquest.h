// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameMode_Breakthrough.h"
#include "GameMode_Conquest.generated.h"

// V2 Conquest mode: every controlled sector continuously earns points for its owner.
// It reuses the multiplayer spawn, character, DataTable, and forbidden-zone pipeline
// from Breakthrough, but activates every anchor simultaneously instead of sequencing.
UCLASS()
class MCPGAMEPROJECT_API AGameMode_Conquest : public AGameMode_Breakthrough
{
	GENERATED_BODY()

public:
	AGameMode_Conquest();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|Conquest")
	float ScoreInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "V2|Conquest")
	int32 PointsPerOwnedSector = 1;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	float ScoreAccumulator = 0.f;
	void AwardConquestScore();
	void FinishConquestMatch(int32 WinnerTeam);
};
