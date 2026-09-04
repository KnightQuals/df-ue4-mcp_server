// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SOLGameMode.generated.h"

class ASOLScavenger;

// Round flow + spawning. Beyond wiring the pawn/controller/HUD classes this
// GameMode owns three things no single actor can decide:
//   1) the match clock (a search-and-extract round has to end, otherwise
//      "extract in time" is not a constraint and the loot has no risk);
//   2) scavenger population (NPCs are spawned in code near the containers
//      rather than placed in the map, so the count is configurable and no
//      level re-save is needed);
//   3) respawning the dead — including the rule that an extracted player
//      never comes back.
//
// Config = Game: the round numbers below can be retuned from
// Config/DefaultGame.ini without recompiling, the same "gameplay numbers live
// in data" principle the loot tables follow.
UCLASS(Config = Game)
class SOL_API ASOLGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASOLGameMode();

	// Match length in seconds. 6 minutes is tuned off the walking distances:
	// the container field is 30-48 m across and the exits sit 80-93 m out, so
	// a full loot run plus a loaded walk to an exit takes roughly 3-4 minutes
	// — enough that greed is punished, not enough to idle.
	UPROPERTY(EditDefaultsOnly, Config, Category = "SOL|Round")
	float MatchSeconds = 360.f;

	// Seconds between dying and respawning.
	UPROPERTY(EditDefaultsOnly, Config, Category = "SOL|Round")
	float RespawnDelay = 5.f;

	// How many scavengers to spawn. They are distributed around the container
	// field, one per container (capped by this number), because a guarded
	// container is what turns "walk up and press F" into a decision.
	UPROPERTY(EditDefaultsOnly, Config, Category = "SOL|Scavengers")
	int32 ScavengerCount = 3;

	// Distance from the guarded container that a scavenger patrols around.
	UPROPERTY(EditDefaultsOnly, Config, Category = "SOL|Scavengers")
	float ScavengerSpawnOffset = 700.f;

	// Extraction is final. GameModeBase never respawns on its own (RestartPlayer
	// has to be called explicitly), but several engine paths do call it —
	// seamless travel, a reconnecting client, any future respawn button. If one
	// of them fires for a player who already banked their loot, that player
	// walks back into the map with an empty bag and the round result already
	// scored, which quietly breaks the whole point of extracting. Denying the
	// restart at the GameMode level makes "已撤离" a hard rule instead of a
	// consequence of nobody happening to call RestartPlayer.
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;

	// Safe respawn. The default implementation picks the first player start it
	// finds, which in the smoke test dropped the respawned player 4.4 m from a
	// scavenger that was already hunting — an unwinnable spawn. This one scores
	// every player start by its distance to the nearest live threat and takes
	// the best, so respawning always lands somewhere quiet.
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	// Called by a player controller when its pawn dies: queues the respawn.
	void ScheduleRespawn(APlayerController* PC);

	// Restarts the whole round in place: containers re-lock and re-roll, world
	// drops are cleared, scavengers respawn, every score resets and everyone
	// gets a fresh pawn. Requested by any player from the settlement screen.
	//
	// Without this a finished round meant killing the process and relaunching
	// three windows — for a demo that is played over and over, "press R to run
	// it again" is worth more than most gameplay features.
	void ResetRound();

protected:
	virtual void BeginPlay() override;

	// Spawns the scavenger population around the containers.
	void SpawnScavengers();

	// 1 Hz match clock. Runs on a timer rather than Tick: the whole job is to
	// decrement an integer and broadcast it, and a per-frame float would spend
	// bandwidth replicating sub-second noise (the same trick the battlefield
	// countdown used).
	void TickMatchClock();

	// Time is up: everybody still in the field loses what they were carrying.
	void FinishMatchOnTimeout();

	FTimerHandle MatchClockTimer;
};
