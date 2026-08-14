// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameMode_Conquest.h"
#include "BattleGameState.h"
#include "BattleSectorAnchor.h"
#include "BattleSectorBase.h"
#include "BreakthroughCharacter.h"
#include "SpawnAreaHub.h"
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
	// Round-based scoring (not continuous trickle): a "bell" every 60 seconds
	// snapshots the current ownership of both sectors and awards 2-0 / 1-1 / 0-2.
	RoundSeconds = FMath::Max(5.f, ActiveMapConfig.ConquestRoundSeconds);
	TotalRoundCount = FMath::Max(1, ActiveMapConfig.ConquestRounds);
	RoundTimeRemaining = RoundSeconds;
	LastBroadcastSecond = -1;
	if (ABattleGameState* BattleState = GetGameState<ABattleGameState>())
	{
		BattleState->AttackerScore = 0;
		BattleState->DefenderScore = 0;
		BattleState->bConquestOver = false;
		BattleState->ConquestWinner = -1;
		BattleState->CurrentRound = 1;
		BattleState->TotalRounds = TotalRoundCount;
		BattleState->LastRoundAtkGain = 0;
		BattleState->LastRoundDefGain = 0;
		BattleState->RemainingMatchSeconds = FMath::CeilToInt(RoundTimeRemaining);
		BattleState->ForceNetUpdate();
	}
	for (TActorIterator<ABattleSectorAnchor> It(GetWorld()); It; ++It)
	{
		It->SetSectorActive(true);
	}
	UE_LOG(LogTemp, Warning, TEXT("V2 CONQUEST started: %d round(s), %.0fs each; bell scoring by current sector ownership"),
		TotalRoundCount, RoundSeconds);
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

	RoundTimeRemaining -= DeltaTime;
	const int32 WholeSeconds = FMath::Max(0, FMath::CeilToInt(RoundTimeRemaining));
	if (WholeSeconds != LastBroadcastSecond)
	{
		LastBroadcastSecond = WholeSeconds;
		BattleState->RemainingMatchSeconds = WholeSeconds;
		BattleState->ForceNetUpdate();
	}

	// The bell: score ONLY at the end of a round, never each second while holding.
	if (RoundTimeRemaining <= 0.f)
	{
		ResolveCurrentRound();
	}
}

void AGameMode_Conquest::ResolveCurrentRound()
{
	ABattleGameState* BattleState = GetGameState<ABattleGameState>();
	if (!BattleState || BattleState->bConquestOver)
	{
		return;
	}

	int32 AttackerOwned = 0;
	int32 DefenderOwned = 0;
	for (TActorIterator<ABattleSectorAnchor> It(GetWorld()); It; ++It)
	{
		ABattleSectorAnchor* Anchor = *It;
		if (!Anchor || !Anchor->bIsActive)
		{
			continue;
		}
		if (Anchor->OwningTeam == 0)
		{
			++AttackerOwned;
		}
		else if (Anchor->OwningTeam == 1)
		{
			++DefenderOwned;
		}
	}

	// A sector is worth exactly one point when the bell rings. There are two
	// configured anchors: 2-0 when one team owns both, otherwise 1-1.
	BattleState->LastRoundAtkGain = AttackerOwned;
	BattleState->LastRoundDefGain = DefenderOwned;
	BattleState->AddScore(0, AttackerOwned);
	BattleState->AddScore(1, DefenderOwned);
	UE_LOG(LogTemp, Warning, TEXT("CONQUEST ROUND %d/%d BELL: +%d attackers, +%d defenders; total %d:%d"),
		BattleState->CurrentRound, BattleState->TotalRounds, AttackerOwned, DefenderOwned,
		BattleState->AttackerScore, BattleState->DefenderScore);

	if (BattleState->CurrentRound >= BattleState->TotalRounds)
	{
		if (BattleState->AttackerScore > BattleState->DefenderScore)
		{
			FinishConquestMatch(0);
		}
		else if (BattleState->DefenderScore > BattleState->AttackerScore)
		{
			FinishConquestMatch(1);
		}
		else
		{
			FinishConquestMatch(2);
		}
		return;
	}

	// New round = fresh mini-match. Everyone returns to base and both objectives
	// reset to defender ownership, so neither side carries a capture forward.
	++BattleState->CurrentRound;
	ResetBattlefieldForNextRound();
	RoundTimeRemaining = RoundSeconds;
	LastBroadcastSecond = -1;
	BattleState->RemainingMatchSeconds = FMath::CeilToInt(RoundTimeRemaining);
	BattleState->ForceNetUpdate();
}

void AGameMode_Conquest::ResetBattlefieldForNextRound()
{
	if (!HasAuthority())
	{
		return;
	}

	// Reset players and the single-player defender AI alike. We choose a fresh spawn
	// point from the team's hub, exactly like the initial GameMode spawn pipeline.
	for (TActorIterator<ABreakthroughCharacter> It(GetWorld()); It; ++It)
	{
		ABreakthroughCharacter* Character = *It;
		if (!Character)
		{
			continue;
		}

		ASpawnAreaHub* Hub = Character->Team == 0 ? AttackerHub : DefenderHub;
		if (!Hub)
		{
			// Defensive fallback should a Hub have been deleted/renamed in a custom map.
			for (TActorIterator<ASpawnAreaHub> HubIt(GetWorld()); HubIt; ++HubIt)
			{
				if (HubIt->Team == Character->Team)
				{
					Hub = *HubIt;
					break;
				}
			}
		}

		FVector SpawnLocation = Character->Team == 0 ? FVector(-3000.f, 0.f, 100.f) : FVector(3000.f, 0.f, 100.f);
		FRotator SpawnRotation = FRotator::ZeroRotator;
		if (Hub)
		{
			if (AActor* SpawnPoint = Hub->GetRandomSpawnPoint())
			{
				SpawnLocation = SpawnPoint->GetActorLocation();
				SpawnRotation = SpawnPoint->GetActorRotation();
			}
			else
			{
				SpawnLocation = Hub->GetActorLocation() + FVector(0.f, 0.f, 100.f);
			}
		}
		Character->ResetForNewRound(SpawnLocation, SpawnRotation);
	}

	// Run this after teleports so any departure-overlap callbacks cannot leave stale
	// zone headcounts. Every objective returns to its fresh defender-owned state.
	for (TActorIterator<ABattleSectorAnchor> It(GetWorld()); It; ++It)
	{
		if (ABattleSectorAnchor* Anchor = *It)
		{
			Anchor->ResetToInitialState();
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("CONQUEST ROUND RESET: all characters returned to team bases; all sectors restored to defenders"));
}

void AGameMode_Conquest::FinishConquestMatch(int32 WinnerTeam)
{
	ABattleGameState* BattleState = GetGameState<ABattleGameState>();
	if (!BattleState || BattleState->bConquestOver)
	{
		return;
	}
	BattleState->bConquestOver = true;
	BattleState->ConquestWinner = WinnerTeam;
	BattleState->ForceNetUpdate();
	const TCHAR* Winner = WinnerTeam == 0 ? TEXT("ATTACKERS") : (WinnerTeam == 1 ? TEXT("DEFENDERS") : TEXT("NOBODY (DRAW)"));
	UE_LOG(LogTemp, Warning, TEXT("V2 CONQUEST OVER: %s win (%d:%d)"), Winner,
		BattleState->AttackerScore, BattleState->DefenderScore);
	// Result renders through ABattleHUD (center-screen banner), no debug text.
}
