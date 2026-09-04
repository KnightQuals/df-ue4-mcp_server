// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLGameState.h"
#include "SOL.h"
#include "SOLPlayerState.h"
#include "Net/UnrealNetwork.h"

ASOLGameState::ASOLGameState()
{
	// Round summary changes a handful of times per match, so the default
	// update frequency is plenty — no need to spend bandwidth on it.
}

void ASOLGameState::RefreshRoundState()
{
	// Server authoritative: the count is derived from the server's own
	// player array, never from a client claim.
	if (!HasAuthority())
	{
		return;
	}

	int32 Extracted = 0;
	int32 Total = 0;
	int32 Value = 0;
	for (const APlayerState* PS : PlayerArray)
	{
		const ASOLPlayerState* SOLPS = Cast<ASOLPlayerState>(PS);
		if (!SOLPS)
		{
			continue;
		}
		++Total;
		if (SOLPS->bExtracted)
		{
			++Extracted;
			Value += SOLPS->ExtractedValue;
		}
	}

	ExtractedCount = Extracted;
	TotalPlayers = Total;
	TeamValue = Value;

	// Round over when everybody is out. Guarded on Total > 0 so an empty
	// player array (a dedicated server between connections) does not declare
	// the round finished before anyone joined.
	const bool bAllOut = (Total > 0 && Extracted >= Total);
	if (bAllOut && !bRoundOver)
	{
		bRoundOver = true;
		UE_LOG(LogSOL, Log, TEXT("ROUND OVER: %d/%d extracted, team value %d"),
			Extracted, Total, Value);
	}

	UE_LOG(LogSOL, Log, TEXT("RoundState: %d/%d extracted, team value %d"),
		Extracted, Total, Value);
}

void ASOLGameState::StartMatchClock(float TotalSeconds)
{
	if (!HasAuthority())
	{
		return;
	}
	MatchDurationSeconds = FMath::Max(1, FMath::RoundToInt(TotalSeconds));
	RemainingSeconds = MatchDurationSeconds;
	bTimedOut = false;

	UE_LOG(LogSOL, Log, TEXT("Match clock started: %ds"), RemainingSeconds);
}

bool ASOLGameState::AdvanceMatchClock(int32 DeltaSeconds)
{
	if (!HasAuthority() || bRoundOver)
	{
		return false;
	}

	RemainingSeconds = FMath::Max(0, RemainingSeconds - FMath::Max(1, DeltaSeconds));

	// Log the last few seconds and every half minute — enough to prove the
	// clock ran without flooding the log for six minutes.
	if (RemainingSeconds <= 5 || RemainingSeconds % 30 == 0)
	{
		UE_LOG(LogSOL, Log, TEXT("Match clock: %ds left"), RemainingSeconds);
	}

	return RemainingSeconds > 0;
}

void ASOLGameState::FinishRound()
{
	if (!HasAuthority())
	{
		return;
	}
	RemainingSeconds = 0;
	bTimedOut = true;
	bRoundOver = true;

	// Recount so the settlement board shows the final tallies (who did get
	// out before the clock stopped).
	RefreshRoundState();

	UE_LOG(LogSOL, Log, TEXT("ROUND OVER (timeout): %d/%d extracted, team value %d"),
		ExtractedCount, TotalPlayers, TeamValue);
}

void ASOLGameState::ResetRoundState()
{
	if (!HasAuthority())
	{
		return;
	}

	// Clearing bRoundOver is what makes the settlement board disappear on
	// every client — the HUD polls this flag, so no extra notification is
	// needed for the reset to be visible.
	bRoundOver = false;
	bTimedOut = false;
	ExtractedCount = 0;
	TeamValue = 0;

	UE_LOG(LogSOL, Log, TEXT("GameState round flags cleared for a new round"));
}

void ASOLGameState::OnRep_bRoundOver()
{
	// Presentation-only: the HUD polls bRoundOver. Logged as replication
	// evidence for the verification pass.
	UE_LOG(LogSOL, Log, TEXT("OnRep_bRoundOver: %s (%d/%d, team %d)"),
		bRoundOver ? TEXT("true") : TEXT("false"), ExtractedCount, TotalPlayers, TeamValue);
}

void ASOLGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASOLGameState, ExtractedCount);
	DOREPLIFETIME(ASOLGameState, TotalPlayers);
	DOREPLIFETIME(ASOLGameState, bRoundOver);
	DOREPLIFETIME(ASOLGameState, TeamValue);
	DOREPLIFETIME(ASOLGameState, RemainingSeconds);
	DOREPLIFETIME(ASOLGameState, MatchDurationSeconds);
	DOREPLIFETIME(ASOLGameState, bTimedOut);
}
