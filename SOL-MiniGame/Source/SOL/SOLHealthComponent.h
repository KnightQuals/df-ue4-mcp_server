// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SOLHealthComponent.generated.h"

// Broadcast on the server the moment health reaches zero. The owner decides
// what dying means (players drop their loot and respawn, scavengers drop and
// disappear) — this component only owns the number and the authority check.
DECLARE_DELEGATE_TwoParams(FSOLDeathSignature, AActor* /*Victim*/, AController* /*Killer*/);

// Health for anything that can be shot: players and scavenger NPCs share this
// component, so damage adjudication exists in exactly one place.
//
// Two deliberate design points:
//  1) It plugs into the engine's own damage pipeline (AActor::TakeDamage ->
//     OnTakeAnyDamage) instead of inventing a parallel one. Weapons call
//     UGameplayStatics::ApplyPointDamage; whether the victim is a player or an
//     NPC is irrelevant to the shooter.
//  2) Health replicates to everyone, not just the owner. The HUD draws enemy
//     health above nearby scavengers, and a hit needs to read as a hit on
//     every client — unlike the backpack, current health is not private.
UCLASS(ClassGroup = (SOL), meta = (BlueprintSpawnableComponent))
class SOL_API USOLHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USOLHealthComponent();

	UPROPERTY(EditAnywhere, Category = "SOL|Health")
	float MaxHealth = 100.f;

	// Current health. Replicated to all: enemy nameplates and hit feedback
	// both need it, and it is not secret information.
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_Health, Category = "SOL|Health")
	float Health = 100.f;

	// Set once health hits zero, so every later damage event is ignored and
	// the death handler can never run twice (a burst of bullets arriving in
	// the same frame would otherwise settle the same corpse repeatedly).
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SOL|Health")
	bool bDead = false;

	// Regeneration. There are no medkits in v1 (the medical items are pure
	// loot value), so without any recovery a single firefight would end a
	// player's run permanently. Slow out-of-combat regen keeps the round
	// going without making bullets meaningless.
	UPROPERTY(EditAnywhere, Category = "SOL|Health|Regen")
	float RegenPerSecond = 4.f;

	// How long after the last hit regeneration starts.
	UPROPERTY(EditAnywhere, Category = "SOL|Health|Regen")
	float RegenDelay = 6.f;

	float GetHealthRatio() const { return MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f; }

	bool IsDead() const { return bDead; }

	// Spawn protection. Respawning is server-driven and drops the player back
	// at a player start that a scavenger may already be walking towards; the
	// smoke test hit exactly that ("acquired SOLCharacter_1 at 437cm" right
	// after a respawn), which is an unwinnable spawn. A few invulnerable
	// seconds turn that into a chance to move instead of a second death.
	UPROPERTY(EditAnywhere, Category = "SOL|Health|Spawn")
	float SpawnProtectSeconds = 3.f;

	// Replicated so the owning client's HUD can say why it is not taking
	// damage, and so remote clients could tint a protected player.
	UPROPERTY(VisibleAnywhere, Replicated, Category = "SOL|Health|Spawn")
	float ProtectionLeft = 0.f;

	bool IsProtected() const { return ProtectionLeft > 0.f; }

	// Server-side: start the spawn protection window (called from BeginPlay).
	void BeginSpawnProtection();

	// Server-side: full heal and clear the dead flag (used on respawn, and by
	// the scavenger spawner when it reuses a pooled actor).
	void ResetHealth();

	// Owner-facing death hook. Bound in the owner's BeginPlay; only ever
	// fired on the authority.
	FSOLDeathSignature OnDeath;

	// Seconds since the last damage event (drives the HUD's "in combat"
	// state as well as regeneration).
	float GetTimeSinceLastDamage() const { return TimeSinceLastDamage; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Engine damage pipeline entry point (bound to the owner's
	// OnTakeAnyDamage in BeginPlay, server only).
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType,
		class AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void OnRep_Health();

private:
	float TimeSinceLastDamage = 999.f;

	// Client-side: remembers the last replicated value so OnRep can tell a
	// hit (health went down) from a respawn/regen (health went up).
	float LastKnownHealth = 100.f;
};
