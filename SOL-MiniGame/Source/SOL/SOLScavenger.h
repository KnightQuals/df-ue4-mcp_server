// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SOLCharacter.h"
#include "SOLScavenger.generated.h"

// What the AI is currently doing. Two states only: a search-and-extract
// scavenger either wanders its patch or shoots at someone. Anything more
// (cover, flanking, regrouping) would be a behaviour-tree project of its own
// and buys nothing for a v1 whose point is "there is someone out there".
UENUM()
enum class ESOLScavState : uint8
{
	Patrol,
	Combat
};

// Scavenger NPC — the "/NPC" half of the design doc's core loop
// ("击败其他玩家/NPC搜刮物资"). Subclassing ASOLCharacter is the whole trick:
// health, the damage pipeline, death, the backpack spill and the three-band
// locomotion animation are all inherited, so adding an enemy costs no
// duplicated gameplay code — only perception and a movement decision.
//
// No NavMesh and no behaviour tree, for the same reason the battlefield
// project skipped them: the map is open ground, so "walk towards a random
// point" is the entire navigation requirement. The price is that a scavenger
// will not path around an obstacle — it bumps into it and picks a new target
// on the next cycle, which on flat terrain is invisible.
UCLASS()
class SOL_API ASOLScavenger : public ASOLCharacter
{
	GENERATED_BODY()

public:
	ASOLScavenger();

	// ------------------------------------------------------------ perception
	// 26 m sight. The containers sit 19 m apart, so this is deliberately just
	// over one container spacing: a player can be seen by the guard of the
	// crate they are looting and possibly its neighbour, never by half the map.
	// The first smoke test ran with 35 m and three scavengers converged on a
	// stationary player from across the field — unwinnable, and not the kind of
	// pressure this loop wants.
	UPROPERTY(EditAnywhere, Category = "SOL|Scavenger|Perception")
	float SightRange = 2000.f;

	// Half-angle of the vision cone, in degrees. 70 means a 140-degree field
	// of view: generous enough to be threatening, narrow enough to flank.
	UPROPERTY(EditAnywhere, Category = "SOL|Scavenger|Perception")
	float SightHalfAngleDeg = 70.f;

	// How long the scavenger keeps hunting after losing sight of its target.
	UPROPERTY(EditAnywhere, Category = "SOL|Scavenger|Perception")
	float LoseTargetSeconds = 5.f;

	// Only pawns within this distance of the guarded container are treated as
	// hostile (15 m). The guard ignores everyone else no matter where its
	// patrol wanders — this is what keeps the spawn area safe by construction
	// instead of by tuning luck.
	UPROPERTY(EditAnywhere, Category = "SOL|Scavenger|Perception")
	float GuardRadius = 1500.f;

	// Maximum distance it will chase away from the container it guards. A
	// scavenger is a guard, not a hunter: without a leash it followed the
	// player across the whole map in the first smoke test, which both emptied
	// the container field and made disengaging impossible.
	//
	// The value has to clear the whole chase, not just the patrol: 900 cm of
	// patrol drift plus roughly 1200 cm of closing to firing range already puts
	// it at 2100 cm. The first attempt at 2400 cm sat right on that boundary
	// and every scavenger disengaged *before firing a single shot* (the second
	// smoke test logged four leash events and zero SHOT lines). 3200 cm leaves
	// room for one complete engagement while still stopping a cross-map chase.
	UPROPERTY(EditAnywhere, Category = "SOL|Scavenger|Perception")
	float LeashRange = 2600.f;

	// ---------------------------------------------------------------- combat
	// Distance it tries to hold while shooting. Closer than this it stops
	// advancing, so it does not walk into the player's face.
	UPROPERTY(EditAnywhere, Category = "SOL|Scavenger|Combat")
	float PreferredCombatRange = 1400.f;

	// Aim error cone. Without it a hitscan AI is a laser: three shots and the
	// player is dead with no counterplay. 5 degrees at 14 m is roughly a
	// 1.2 m spread — threatening, survivable, and it rewards moving.
	UPROPERTY(EditAnywhere, Category = "SOL|Scavenger|Combat")
	float AimSpreadDeg = 5.f;

	// ---------------------------------------------------------------- patrol
	UPROPERTY(EditAnywhere, Category = "SOL|Scavenger|Patrol")
	float PatrolRadius = 900.f;

	UPROPERTY(EditAnywhere, Category = "SOL|Scavenger|Patrol")
	float PatrolPauseMin = 2.f;

	UPROPERTY(EditAnywhere, Category = "SOL|Scavenger|Patrol")
	float PatrolPauseMax = 4.5f;

	// ------------------------------------------------------------------ loot
	// Value tier of what this scavenger carries, rolled at BeginPlay. Killing
	// one is worth looting: the corpse spills a real (if modest) stack, which
	// is what makes engaging an NPC pay for the ammo and the noise.
	UPROPERTY(EditAnywhere, Category = "SOL|Scavenger|Loot")
	int32 LootStacks = 2;

	// Where it patrols around. Set by the spawner; defaults to wherever it
	// was placed.
	UPROPERTY(EditAnywhere, Category = "SOL|Scavenger|Patrol")
	FVector HomeLocation = FVector::ZeroVector;

	// Chinese nameplate label shown by the HUD (with a health bar) — the only
	// way to tell a scavenger from a player, since both use SK_Mannequin.
	UPROPERTY(EditAnywhere, Category = "SOL|Scavenger")
	FString DisplayNameZh = TEXT("拾荒者");

	// Fires along the facing direction from eye height, with the aim cone
	// applied — a scavenger has no camera to shoot from.
	virtual void GetAimRay(FVector& OutStart, FVector& OutDir) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void HandleDeath(AActor* Victim, AController* Killer) override;

private:
	void UpdatePerception();
	void TickPatrol(float DeltaSeconds);
	void TickCombat(float DeltaSeconds);
	void PickNewPatrolPoint();
	void FaceLocation(const FVector& Target, float DeltaSeconds);
	bool CanSee(const class ASOLCharacter* Other) const;
	void RollLoot();

	ESOLScavState State = ESOLScavState::Patrol;

	TWeakObjectPtr<ASOLCharacter> Target;
	float TimeSinceSeenTarget = 0.f;

	FVector PatrolPoint = FVector::ZeroVector;
	float PatrolPauseLeft = 0.f;

	// Perception runs at 4 Hz, not every frame: a 0.25 s reaction delay is
	// both cheaper and more human than an instant one.
	float SenseAccumulator = 0.f;
};
