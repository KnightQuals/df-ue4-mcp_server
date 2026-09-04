// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SOLCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class USOLBackpackComponent;
class USOLHealthComponent;

// Minimal first-person pawn: capsule + eye-height camera + legacy axis input
// (WASD / mouse turn / space jump) declared in DefaultInput.ini. The third
// person body uses SK_Mannequin (AnimStarterPack, same as the battlefield
// project) so remote players see a recognizable human; OwnerNoSee keeps it
// hidden from the local first-person camera.
//
// Combat lives here rather than in a separate weapon actor: the design doc
// defines SOL as "进入地图，击败其他玩家/NPC搜刮物资，撤退", and for a v1 with a
// single hitscan weapon a whole weapon-actor hierarchy would be ceremony. The
// scavenger NPC subclasses this pawn, so players and NPCs shoot, bleed and
// drop loot through exactly the same code path.
UCLASS()
class SOL_API ASOLCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASOLCharacter();

	UPROPERTY(VisibleAnywhere, Category = "SOL|Character")
	UCameraComponent* FirstPersonCamera = nullptr;

	// Skeletal mesh body visible to other players (and hidden from the
	// owning player's first-person view via OwnerNoSee).
	UPROPERTY(VisibleAnywhere, Category = "SOL|Character")
	USkeletalMeshComponent* BodyMesh = nullptr;

	// Weight-limited inventory, shared by HUD panels and the drop/pickup loop.
	UPROPERTY(VisibleAnywhere, Category = "SOL|Character")
	USOLBackpackComponent* Backpack = nullptr;

	// Health + damage adjudication (shared with the scavenger NPC).
	UPROPERTY(VisibleAnywhere, Category = "SOL|Character")
	USOLHealthComponent* HealthComp = nullptr;

	// ---------------------------------------------------------------- weapon
	UPROPERTY(EditAnywhere, Category = "SOL|Weapon")
	float WeaponDamage = 34.f;

	// 80 m. Long enough to cover the container field, short enough that the
	// 145 m gaps between extraction zones are not a shooting gallery.
	UPROPERTY(EditAnywhere, Category = "SOL|Weapon")
	float WeaponRange = 8000.f;

	UPROPERTY(EditAnywhere, Category = "SOL|Weapon")
	float FireInterval = 0.15f;

	UPROPERTY(EditAnywhere, Category = "SOL|Weapon")
	int32 MagazineSize = 30;

	UPROPERTY(EditAnywhere, Category = "SOL|Weapon")
	float ReloadSeconds = 1.8f;

	// Rounds left in the magazine. Owner-only: nobody else needs to know how
	// many bullets you have. Reloading is free (no ammo item is consumed) —
	// v1 deliberately avoids a "out of ammo, run cannot continue" dead end.
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_Ammo, Category = "SOL|Weapon")
	int32 CurrentAmmo = 30;

	UPROPERTY(VisibleAnywhere, Replicated, Category = "SOL|Weapon")
	bool bReloading = false;

	// Local input entry points (called by the player controller; the AI calls
	// TryFire directly on the server).
	void TryFire();
	void TryReload();

	bool IsDead() const;

	// HUD readouts.
	float GetHealthRatio() const;
	int32 GetCurrentAmmo() const { return CurrentAmmo; }
	bool IsReloading() const { return bReloading; }

	// Where a shot starts and which way it points. The player uses the camera;
	// the scavenger overrides it to fire from eye height along its facing,
	// since an AI pawn has no camera of its own.
	virtual void GetAimRay(FVector& OutStart, FVector& OutDir) const;

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void MoveForward(float Value);
	void MoveRight(float Value);

	// Server authoritative shot. The client sends where it was aiming (the
	// server cannot know the client's exact view rotation), and the server
	// sanity-checks the origin against its own copy of the pawn before
	// tracing — a tampered client can skew its aim, not shoot from across
	// the map or through geometry.
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerFireShot(FVector_NetQuantize TraceStart, FVector_NetQuantizeNormal TraceDir);

	UFUNCTION(Server, Reliable)
	void ServerReload();

	// Tracer, for everybody. Unreliable on purpose: a dropped tracer is a
	// missing visual, never a missing hit — the hit itself was already
	// resolved on the server.
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotFired(FVector_NetQuantize From, FVector_NetQuantize To);

	// Shooter-side confirmation, so the crosshair can react.
	UFUNCTION(Client, Reliable)
	void ClientHitFeedback(bool bKilled, const FString& VictimName, int32 Bounty);

	UFUNCTION()
	void OnRep_Ammo();

	// Server-side death handling. Base implementation spills the backpack into
	// the world and credits the killer; the player subclass path also queues a
	// respawn, the scavenger removes itself.
	virtual void HandleDeath(AActor* Victim, AController* Killer);

	// Scatters everything in the backpack around the corpse as world pickups.
	// This is the whole reason combat exists in a search-and-extract round:
	// dying means the run's loot goes back on the floor for someone else,
	// which is what makes reaching an extraction zone worth anything.
	void SpillBackpack();

	void FinishReload();

	FTimerHandle ReloadTimer;

	// Server-side rate limit (the client also limits itself locally so the
	// UI feels right, but the server never trusts that).
	float LastFireServerTime = -10.f;

	// Client-side copy of the same limit, purely so trigger spam does not
	// flood the server with RPCs it will reject anyway.
	float LastFireLocalTime = -10.f;

private:
	// Locomotion animation, driven directly from C++ rather than through
	// AnimStarterPack's AnimBlueprint: that blueprint casts to its own
	// Ue4ASP_Character class, which ASOLCharacter is not, so the cast fails and
	// the state machine never leaves idle (the battlefield project hit the same
	// wall and also fell back to explicit PlayAnimation).
	//
	// Three speed bands instead of two, because the encumbrance system makes
	// speed meaningful: a 满载 player physically walks, a light one jogs. The
	// animation therefore reads the load without any extra UI.
	void UpdateLocomotionAnimation();

	UPROPERTY() class UAnimSequence* IdleAnim = nullptr;
	UPROPERTY() class UAnimSequence* WalkAnim = nullptr;
	UPROPERTY() class UAnimSequence* JogAnim = nullptr;
	UPROPERTY() class UAnimSequence* DeathAnim = nullptr;

	// Which clip is currently looping, so we only call PlayAnimation on change
	// (restarting the same clip every frame would freeze it on frame 0).
	class UAnimSequence* CurrentAnim = nullptr;
};
