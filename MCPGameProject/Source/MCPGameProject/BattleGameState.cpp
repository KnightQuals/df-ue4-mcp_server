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
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9100, 1.2f, FColor::White,
			FString::Printf(TEXT("CONQUEST  ATTACK %d  :  %d DEFEND"), AttackerScore, DefenderScore),
			true, FVector2D(1.35f, 1.35f));
	}
}
