// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLGameMode.h"
#include "SOL.h"
#include "SOLCharacter.h"
#include "SOLContainer.h"
#include "SOLScavenger.h"
#include "SOLItemPickup.h"
#include "SOLPlayerController.h"
#include "SOLPlayerState.h"
#include "SOLGameState.h"
#include "SOLBackpackComponent.h"
#include "SOLHUD.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"

ASOLGameMode::ASOLGameMode()
{
	DefaultPawnClass = ASOLCharacter::StaticClass();
	PlayerControllerClass = ASOLPlayerController::StaticClass();
	HUDClass = ASOLHUD::StaticClass();

	// Round results (extraction flag + payout) live on the PlayerState so
	// they survive pawn destruction at extraction time and replicate to
	// every client for the settlement board.
	PlayerStateClass = ASOLPlayerState::StaticClass();

	// GameState carries the round summary (how many are out, team value,
	// round-over flag, match clock). GameMode itself only exists on the
	// server, so any state a client HUD must read has to live here instead.
	GameStateClass = ASOLGameState::StaticClass();
}

void ASOLGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	// Seed the clock on the GameState (the client-readable copy) and start
	// ticking it once per second.
	if (ASOLGameState* GS = GetGameState<ASOLGameState>())
	{
		GS->StartMatchClock(MatchSeconds);
	}
	GetWorldTimerManager().SetTimer(MatchClockTimer, this, &ASOLGameMode::TickMatchClock, 1.f, true);

	SpawnScavengers();

	UE_LOG(LogSOL, Log, TEXT("Match started: %.0fs, %d scavenger(s) requested"),
		MatchSeconds, ScavengerCount);
}

void ASOLGameMode::SpawnScavengers()
{
	UWorld* World = GetWorld();
	if (!World || ScavengerCount <= 0)
	{
		return;
	}

	// Collect the containers first: guarding loot is the point, so the NPC
	// positions are derived from where the loot is instead of hardcoded. This
	// also means the layout retunes itself whenever containers move.
	TArray<AActor*> Guarded;
	for (TActorIterator<ASOLContainer> It(World); It; ++It)
	{
		if (*It)
		{
			Guarded.Add(*It);
		}
	}
	if (Guarded.Num() == 0)
	{
		UE_LOG(LogSOL, Warning, TEXT("SpawnScavengers: no containers found, skipping"));
		return;
	}

	// Deterministic ordering (actor iteration order is not guaranteed stable
	// across runs) keeps demos reproducible. Farthest-from-centre first:
	// with fewer scavengers than containers, the guards belong on the far
	// loot, leaving the near containers safe for the opening minute.
	Guarded.Sort([](const AActor& A, const AActor& B) {
		return A.GetActorLocation().SizeSquared2D() > B.GetActorLocation().SizeSquared2D();
	});

	// Where the players already are. Scavengers spawn after the first player
	// has been given a pawn (GameMode::BeginPlay runs after PostLogin), so
	// without this check a guard can materialise inside the spawn area and
	// open fire before the player has moved — the second smoke test had three
	// of them looking at a stationary player from the edge of their sight.
	TArray<FVector> PlayerSpots;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* PC = Cast<APlayerController>(It->Get()))
		{
			if (const APawn* Pawn = PC->GetPawn())
			{
				PlayerSpots.Add(Pawn->GetActorLocation());
			}
		}
	}
	auto TooCloseToPlayer = [&PlayerSpots](const FVector& Candidate, float& OutNearest)
	{
		OutNearest = TNumericLimits<float>::Max();
		for (const FVector& Spot : PlayerSpots)
		{
			// 12 m no-spawn bubble. This used to be one sight radius (26 m),
			// but the compact demo layout puts containers 17-26 m out, which
			// made that rule skip every candidate (0 spawned). 12 m still
			// guarantees nobody materialises on top of the spawn area, and
			// the squad spawn logic keeps players away from live guards.
			const float D = FVector::Dist2D(Candidate, Spot);
			OutNearest = FMath::Min(OutNearest, D);
			if (D < 1200.f)
			{
				return true;
			}
		}
		return false;
	};

	// Diagnostic: without these two lines the skip logic is a black box and
	// every tuning pass flies blind (2026-09-03: two wasted rounds guessing).
	for (const FVector& Spot : PlayerSpots)
	{
		UE_LOG(LogSOL, Log, TEXT("SpawnScavengers: player spot at (%.0f, %.0f)"), Spot.X, Spot.Y);
	}

	const int32 Count = FMath::Min(ScavengerCount, Guarded.Num());
	for (int32 Idx = 0; Idx < Count; ++Idx)
	{
		const FVector P = Guarded[Idx]->GetActorLocation();
		UE_LOG(LogSOL, Log, TEXT("SpawnScavengers: candidate %d = %s at (%.0f, %.0f)"),
			Idx, *Guarded[Idx]->GetName(), P.X, P.Y);
	}
	int32 Spawned = 0;
	int32 Skipped = 0;
	for (int32 Idx = 0; Idx < Count; ++Idx)
	{
		const AActor* Anchor = Guarded[Idx];
		// Offset so the scavenger stands beside the container rather than
		// inside its interaction sphere (a player must be able to reach the
		// container without spawning nose-to-nose with the guard).
		const float Angle = (2.f * PI * Idx) / FMath::Max(1, Count);
		FVector Offset(FMath::Cos(Angle) * ScavengerSpawnOffset,
			FMath::Sin(Angle) * ScavengerSpawnOffset, 120.f);
		FVector Where = Anchor->GetActorLocation() + Offset;

		float NearestA = 0.f, NearestB = 0.f;
		if (TooCloseToPlayer(Where, NearestA))
		{
			// Try the far side of the same container first — the container is
			// still guarded, just from the other end.
			Where = Anchor->GetActorLocation() - FVector(Offset.X, Offset.Y, -120.f);
			if (TooCloseToPlayer(Where, NearestB))
			{
				++Skipped;
				UE_LOG(LogSOL, Log, TEXT("SpawnScavengers: skipping %s at (%.0f, %.0f), nearest player %.0fcm / %.0fcm"),
					*Anchor->GetName(), Anchor->GetActorLocation().X, Anchor->GetActorLocation().Y, NearestA, NearestB);
				continue;
			}
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ASOLScavenger* Scav = World->SpawnActor<ASOLScavenger>(
			ASOLScavenger::StaticClass(), Where, FRotator(0.f, Angle * 180.f / PI + 180.f, 0.f), Params);
		if (Scav)
		{
			Scav->HomeLocation = Where;
			++Spawned;
		}
	}

	UE_LOG(LogSOL, Log, TEXT("SpawnScavengers: %d spawned around %d container(s), %d skipped (player nearby)"),
		Spawned, Guarded.Num(), Skipped);
}

void ASOLGameMode::TickMatchClock()
{
	ASOLGameState* GS = GetGameState<ASOLGameState>();
	if (!GS)
	{
		return;
	}

	// The GameState owns the number (clients read it); the GameMode owns what
	// happens when it hits zero.
	if (GS->AdvanceMatchClock(1))
	{
		return; // still running
	}

	GetWorldTimerManager().ClearTimer(MatchClockTimer);
	FinishMatchOnTimeout();
}

void ASOLGameMode::FinishMatchOnTimeout()
{
	ASOLGameState* GS = GetGameState<ASOLGameState>();
	if (!GS)
	{
		return;
	}

	// Everyone still holding loot loses it. This is the risk half of the loop:
	// value only counts once it has been carried out, so an unbanked bag is
	// worth exactly nothing when the clock stops.
	int32 Failed = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = Cast<APlayerController>(It->Get());
		ASOLPlayerState* PS = PC ? PC->GetPlayerState<ASOLPlayerState>() : nullptr;
		if (!PS || PS->bExtracted)
		{
			continue;
		}

		int32 Carried = 0;
		if (const APawn* Pawn = PC->GetPawn())
		{
			if (const USOLBackpackComponent* Pack = Pawn->FindComponentByClass<USOLBackpackComponent>())
			{
				Carried = Pack->CurrentValue();
			}
		}
		PS->MarkExtractionFailed(Carried);
		++Failed;

		if (ASOLPlayerController* SOLPC = Cast<ASOLPlayerController>(PC))
		{
			SOLPC->ClientShowMessage(TEXT("时间耗尽 —— 撤离失败，携带物资全部损失"));
		}
	}

	GS->FinishRound();

	UE_LOG(LogSOL, Log, TEXT("MATCH TIMEOUT: %d player(s) failed to extract"), Failed);
}

AActor* ASOLGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// Collect live threats (scavengers) and living teammates separately. The
	// demo runs a two-player co-op squad, so a teammate is a reason to spawn
	// NEAR a start, not away from it (user feedback 2026-09-03: the two
	// players spawned on opposite sides of the map and never saw each other).
	TArray<FVector> Threats;
	for (TActorIterator<ASOLScavenger> It(World); It; ++It)
	{
		if (*It && !(*It)->IsDead())
		{
			Threats.Add((*It)->GetActorLocation());
		}
	}
	TArray<FVector> Teammates;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Other = Cast<APlayerController>(It->Get());
		if (!Other || Other == Player)
		{
			continue;
		}
		if (const APawn* OtherPawn = Other->GetPawn())
		{
			if (const ASOLCharacter* OtherChar = Cast<ASOLCharacter>(OtherPawn))
			{
				if (!OtherChar->IsDead())
				{
					Teammates.Add(OtherPawn->GetActorLocation());
				}
			}
		}
	}

	APlayerStart* Best = nullptr;
	float BestScore = -1.f;
	bool bBestIsTeamSpawn = false;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!Start)
		{
			continue;
		}

		const float ThreatDist = [&]() {
			float D = TNumericLimits<float>::Max();
			for (const FVector& Threat : Threats)
			{
				D = FMath::Min(D, FVector::Dist2D(Start->GetActorLocation(), Threat));
			}
			return D;
		}();

		if (Teammates.Num() > 0)
		{
			// Squad spawn: closest start to the nearest teammate wins, but a
			// start sitting inside a scavenger's sight range is disqualified —
			// spawning the second player into the NPC that is already shooting
			// the first one would feel cheap.
			float MateDist = TNumericLimits<float>::Max();
			for (const FVector& Mate : Teammates)
			{
				MateDist = FMath::Min(MateDist, FVector::Dist2D(Start->GetActorLocation(), Mate));
			}
			if (ThreatDist > 2600.f && MateDist < 4000.f)
			{
				// Team candidates outrank any threat-only candidate: negate
				// into a large positive score band.
				const float TeamScore = 1.0e9f - MateDist;
				if (TeamScore > BestScore)
				{
					BestScore = TeamScore;
					Best = Start;
					bBestIsTeamSpawn = true;
				}
				continue;
			}
		}

		// Solo/fallback: farthest from the closest threat, as before.
		if (!bBestIsTeamSpawn && ThreatDist > BestScore)
		{
			BestScore = ThreatDist;
			Best = Start;
		}
	}

	if (!Best)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	if (bBestIsTeamSpawn)
	{
		UE_LOG(LogSOL, Log, TEXT("ChoosePlayerStart: %s (squad spawn, %.0fcm from teammate, %d threat(s) avoided)"),
			*Best->GetName(), 1.0e9f - BestScore, Threats.Num());
	}
	else
	{
		UE_LOG(LogSOL, Log, TEXT("ChoosePlayerStart: %s (nearest threat %.0fcm, %d threat(s) considered)"),
			*Best->GetName(), BestScore >= TNumericLimits<float>::Max() ? -1.f : BestScore, Threats.Num());
	}
	return Best;
}

void ASOLGameMode::ResetRound()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	UE_LOG(LogSOL, Log, TEXT("=== ROUND RESET requested ==="));

	// 1) Clear everything lying on the floor: leftovers from last round would
	//    hand the next one free loot.
	int32 ClearedPickups = 0;
	for (TActorIterator<ASOLItemPickup> It(World); It; ++It)
	{
		if (*It)
		{
			(*It)->Destroy();
			++ClearedPickups;
		}
	}

	// 2) Re-lock the containers so the first open rolls fresh loot. Going
	//    through the container's own method keeps the replication (Contents
	//    and bOpened both ship to clients) in one place.
	int32 ResetContainers = 0;
	for (TActorIterator<ASOLContainer> It(World); It; ++It)
	{
		if (*It)
		{
			(*It)->ResetForNewRound();
			++ResetContainers;
		}
	}

	// 3) Wipe and respawn the scavengers. Reusing the corpses would mean
	//    resetting health, AI state, loot and collision — spawning new ones is
	//    both shorter and impossible to get half-right.
	int32 ClearedScavs = 0;
	for (TActorIterator<ASOLScavenger> It(World); It; ++It)
	{
		if (*It)
		{
			(*It)->Destroy();
			++ClearedScavs;
		}
	}

	// 4) Reset the scores *before* respawning: PlayerCanRestart refuses an
	//    extracted or timed-out player, so the flags have to be gone first.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = Cast<APlayerController>(It->Get());
		if (ASOLPlayerState* PS = PC ? PC->GetPlayerState<ASOLPlayerState>() : nullptr)
		{
			PS->ResetForNewRound();
		}
	}

	// 5) Fresh clock (also clears bRoundOver / bTimedOut, so the settlement
	//    board disappears on every client).
	if (ASOLGameState* GS = GetGameState<ASOLGameState>())
	{
		GS->ResetRoundState();
		GS->StartMatchClock(MatchSeconds);
	}
	GetWorldTimerManager().ClearTimer(MatchClockTimer);
	GetWorldTimerManager().SetTimer(MatchClockTimer, this, &ASOLGameMode::TickMatchClock, 1.f, true);

	// 6) Everyone gets a new pawn. Old ones go first so ChoosePlayerStart's
	//    threat scoring does not treat the corpses as threats.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = Cast<APlayerController>(It->Get());
		if (!PC)
		{
			continue;
		}
		if (APawn* Old = PC->GetPawn())
		{
			PC->UnPossess();
			Old->Destroy();
		}
		// Same reason as the respawn path: a cached StartSpot would pin every
		// player to their original spawn for the rest of the session.
		PC->StartSpot = nullptr;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = Cast<APlayerController>(It->Get());
		if (PC && PlayerCanRestart(PC))
		{
			RestartPlayer(PC);
		}
	}

	// 7) Guards last, so they can avoid the player positions chosen above.
	SpawnScavengers();

	UE_LOG(LogSOL, Log, TEXT("ROUND RESET done: %d pickup(s) cleared, %d container(s) re-locked, %d scavenger(s) replaced"),
		ClearedPickups, ResetContainers, ClearedScavs);
}

bool ASOLGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	// An extracted player is out of the round for good: no respawn, no second
	// run with the payout already banked.
	if (const ASOLPlayerState* PS = Player ? Player->GetPlayerState<ASOLPlayerState>() : nullptr)
	{
		if (PS->bExtracted)
		{
			UE_LOG(LogSOL, Log, TEXT("PlayerCanRestart denied: %s already extracted"),
				*PS->GetPlayerName());
			return false;
		}
		if (PS->bFailedExtraction)
		{
			UE_LOG(LogSOL, Log, TEXT("PlayerCanRestart denied: %s round already over"),
				*PS->GetPlayerName());
			return false;
		}
	}
	return Super::PlayerCanRestart_Implementation(Player);
}

void ASOLGameMode::ScheduleRespawn(APlayerController* PC)
{
	if (!PC || !HasAuthority())
	{
		return;
	}

	// Timer instead of an immediate restart: the death screen needs a beat to
	// read, and respawning inside the same frame as the death would put the
	// player back on their feet before the corpse animation even started.
	FTimerHandle Handle;
	FTimerDelegate Del;
	TWeakObjectPtr<APlayerController> WeakPC(PC);
	Del.BindLambda([this, WeakPC]()
	{
		APlayerController* Target = WeakPC.Get();
		if (!Target || !PlayerCanRestart(Target))
		{
			return;
		}
		// Old pawn goes away here rather than at death: keeping the corpse
		// around until respawn is what lets other players see (and loot) the
		// body they just dropped.
		if (APawn* Corpse = Target->GetPawn())
		{
			Target->UnPossess();
			Corpse->Destroy();
		}

		// Clear the cached start spot, or ChoosePlayerStart never runs again.
		// AGameModeBase::FindPlayerStart returns Player->StartSpot whenever it
		// is set, and it gets set on the first spawn — so every later respawn
		// silently reuses the original player start and the threat-scoring
		// logic below is dead code. The smoke test proved it: five respawns,
		// exactly one "ChoosePlayerStart" line in the log.
		Target->StartSpot = nullptr;

		RestartPlayer(Target);
		UE_LOG(LogSOL, Log, TEXT("RESPAWN: %s"), *Target->GetName());
	});
	GetWorldTimerManager().SetTimer(Handle, Del, RespawnDelay, false);
}
