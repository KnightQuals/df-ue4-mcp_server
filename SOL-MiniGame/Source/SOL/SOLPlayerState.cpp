// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLPlayerState.h"
#include "SOL.h"
#include "Net/UnrealNetwork.h"

ASOLPlayerState::ASOLPlayerState()
{
	// PlayerState replicates by default; the round result fields below are
	// small and change once per round, so the default update frequency is
	// more than enough (no need to bump NetUpdateFrequency here).
}

void ASOLPlayerState::MarkExtracted(int32 Value, int32 Stacks, const FString& ZoneName)
{
	// Server authoritative: only the extraction zone (running on the
	// authority) is allowed to bank a result.
	if (!HasAuthority())
	{
		return;
	}

	bExtracted = true;
	ExtractedValue = Value;
	ExtractedStacks = Stacks;
	ExtractedZoneName = ZoneName;

	// The server has no HUD, so log the settlement — this is also the line
	// the verification pass greps for.
	UE_LOG(LogSOL, Log, TEXT("EXTRACTED: %s banked %d (%d stack(s)) at %s"),
		*GetPlayerName(), Value, Stacks, *ZoneName);
}

void ASOLPlayerState::MarkExtractionFailed(int32 CarriedValue)
{
	if (!HasAuthority() || bExtracted || bFailedExtraction)
	{
		return;
	}

	bFailedExtraction = true;
	LostValue = CarriedValue;

	UE_LOG(LogSOL, Log, TEXT("EXTRACT FAILED: %s ran out of time holding %d"),
		*GetPlayerName(), CarriedValue);
}

void ASOLPlayerState::RegisterKill(int32 VictimCarriedValue)
{
	if (!HasAuthority())
	{
		return;
	}

	++Kills;
	LastKillBounty = VictimCarriedValue;
	KillBountyTotal += VictimCarriedValue;

	UE_LOG(LogSOL, Log, TEXT("KILL: %s (total %d), victim was carrying %d"),
		*GetPlayerName(), Kills, VictimCarriedValue);
}

void ASOLPlayerState::RegisterDeath()
{
	if (!HasAuthority())
	{
		return;
	}
	++Deaths;
	UE_LOG(LogSOL, Log, TEXT("DIED: %s (total %d)"), *GetPlayerName(), Deaths);
}

void ASOLPlayerState::ResetForNewRound()
{
	if (!HasAuthority())
	{
		return;
	}

	bExtracted = false;
	bFailedExtraction = false;
	ExtractedValue = 0;
	ExtractedStacks = 0;
	ExtractedZoneName.Reset();
	LostValue = 0;
	Kills = 0;
	Deaths = 0;
	KillBountyTotal = 0;
	LastKillBounty = 0;

	UE_LOG(LogSOL, Log, TEXT("Round reset for %s"), *GetPlayerName());
}

void ASOLPlayerState::OnRep_bExtracted()
{
	// Presentation-only hook: the HUD polls bExtracted every frame, so there
	// is nothing to push here. Logged so a client-side verification pass can
	// prove the flag actually crossed the wire (the same "did replication
	// really happen" evidence trail the container bug needed).
	UE_LOG(LogSOL, Log, TEXT("OnRep_bExtracted: %s extracted=%s value=%d"),
		*GetPlayerName(), bExtracted ? TEXT("true") : TEXT("false"), ExtractedValue);
}

void ASOLPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Everyone sees everyone's result: a scoreboard is public information,
	// unlike backpack contents (which are COND_OwnerOnly).
	DOREPLIFETIME(ASOLPlayerState, bExtracted);
	DOREPLIFETIME(ASOLPlayerState, ExtractedValue);
	DOREPLIFETIME(ASOLPlayerState, ExtractedStacks);
	DOREPLIFETIME(ASOLPlayerState, ExtractedZoneName);
	DOREPLIFETIME(ASOLPlayerState, bFailedExtraction);
	DOREPLIFETIME(ASOLPlayerState, LostValue);
	DOREPLIFETIME(ASOLPlayerState, Kills);
	DOREPLIFETIME(ASOLPlayerState, Deaths);
	DOREPLIFETIME(ASOLPlayerState, KillBountyTotal);
}
