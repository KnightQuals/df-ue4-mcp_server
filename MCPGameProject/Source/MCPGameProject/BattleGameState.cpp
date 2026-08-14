// Copyright Epic Games, Inc. All Rights Reserved.

#include "BattleGameState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

ABattleGameState::ABattleGameState()
{
	bReplicates = true;
}

void ABattleGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABattleGameState, AttackerScore);
	DOREPLIFETIME(ABattleGameState, DefenderScore);
	DOREPLIFETIME(ABattleGameState, bConquestOver);
	DOREPLIFETIME(ABattleGameState, RemainingMatchSeconds);
	DOREPLIFETIME(ABattleGameState, ConquestWinner);
	DOREPLIFETIME(ABattleGameState, CurrentRound);
	DOREPLIFETIME(ABattleGameState, TotalRounds);
	DOREPLIFETIME(ABattleGameState, LastRoundAtkGain);
	DOREPLIFETIME(ABattleGameState, LastRoundDefGain);
}

void ABattleGameState::AddScore(int32 Team, int32 Amount)
{
	if (!HasAuthority() || bConquestOver || Amount <= 0)
	{
		return;
	}
	if (Team == 0)
	{
		AttackerScore += Amount;
	}
	else if (Team == 1)
	{
		DefenderScore += Amount;
	}
	OnRep_Scores(); // listen-server local display; clients receive replication.
	ForceNetUpdate();
}

void ABattleGameState::OnRep_Scores()
{
	// Intentionally empty: scores render through ABattleHUD::DrawHUD now. The OnRep
	// hook stays so blueprints can still bind score-change side effects if needed.
}
