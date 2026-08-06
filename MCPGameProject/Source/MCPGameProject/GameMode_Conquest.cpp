// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameMode_Conquest.h"
#include "BattleGameState.h"
#include "BattleSectorAnchor.h"
#include "BattleSectorBase.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"

AGameMode_Conquest::AGameMode_Conquest()
{
	GameStateClass = ABattleGameState::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void AGameMode_Conquest::BeginPlay()
{
	// Parent loads DataTable, sets spawn hubs, and creates V2 forbidden-zone borders.
	Super::BeginPlay();

	// Breakthrough's sector container may have initialized its sequential objective
	// earlier in startup. Conquest deliberately enables every objective at once.
	for (TActorIterator<ABattleSectorBase> It(GetWorld()); It; ++It)
	{
		It->bUseSequentialSectors = false;
	}
	if (ABattleGameState* BattleState = GetGameState<ABattleGameState>())
	{
		BattleState->ScoreToWin = ActiveMapConfig.ConquestScoreToWin;
	}
	ScoreInterval = ActiveMapConfig.ConquestScoreInterval;
	PointsPerOwnedSector = ActiveMapConfig.ConquestPointsPerOwnedSector;
	for (TActorIterator<ABattleSectorAnchor> It(GetWorld()); It; ++It)
	{
		It->SetSectorActive(true);
	}
	UE_LOG(LogTemp, Warning, TEXT("V2 CONQUEST started: all sectors active, score interval %.1fs"), ScoreInterval);
}

void AGameMode_Conquest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority())
	{
		return;
	}

	ABattleGameState* BattleState = GetGameState<ABattleGameState>();
	if (!BattleState || BattleState->bConquestOver)
	{
		return;
	}

	ScoreAccumulator += DeltaTime;
	if (ScoreAccumulator >= ScoreInterval)
	{
		ScoreAccumulator -= ScoreInterval;
		AwardConquestScore();
	}
}

void AGameMode_Conquest::AwardConquestScore()
{
	ABattleGameState* BattleState = GetGameState<ABattleGameState>();
	if (!BattleState)
	{
		return;
	}

	int32 AttackerOwned = 0;
	int32 DefenderOwned = 0;
	for (TActorIterator<ABattleSectorAnchor> It(GetWorld()); It; ++It)
	{
		if (It->OwningTeam == 0)
		{
			++AttackerOwned;
		}
		else if (It->OwningTeam == 1)
		{
			++DefenderOwned;
		}
	}

	BattleState->AddScore(0, AttackerOwned * PointsPerOwnedSector);
	BattleState->AddScore(1, DefenderOwned * PointsPerOwnedSector);

	if (BattleState->AttackerScore >= BattleState->ScoreToWin)
	{
		FinishConquestMatch(0);
	}
	else if (BattleState->DefenderScore >= BattleState->ScoreToWin)
	{
		FinishConquestMatch(1);
	}
}

void AGameMode_Conquest::FinishConquestMatch(int32 WinnerTeam)
{
	ABattleGameState* BattleState = GetGameState<ABattleGameState>();
	if (!BattleState || BattleState->bConquestOver)
	{
		return;
	}
	BattleState->bConquestOver = true;
	BattleState->ForceNetUpdate();
	const TCHAR* Winner = WinnerTeam == 0 ? TEXT("ATTACKERS") : TEXT("DEFENDERS");
	UE_LOG(LogTemp, Warning, TEXT("V2 CONQUEST OVER: %s win (%d:%d)"), Winner,
		BattleState->AttackerScore, BattleState->DefenderScore);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage((uint64)-1, 8.f,
			WinnerTeam == 0 ? FColor(255, 60, 60) : FColor(60, 140, 255),
			FString::Printf(TEXT(">>> CONQUEST WINNER: %s <<<"), Winner), true, FVector2D(2.f, 2.f));
	}
}
