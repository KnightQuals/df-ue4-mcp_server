// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "BreakthroughCharacter.generated.h"

class UStaticMeshComponent;
class UCameraComponent;

// Forward declarations for Enhanced Input assets.
class UInputMappingContext;
class UInputAction;

// Playable character for the Breakthrough game mode. Team assignment (0 = attacker,
// 1 = defender) drives which side of the capture logic this pawn counts toward.
// V1 simplification: no replication, default character movement component.
// 旧版 Input（BindAxis）生效，但保留 EnhancedInput UPROPERTY 兼容 BP_BreakthroughCharacter 蓝图变量引用。
UCLASS()
class MCPGAMEPROJECT_API ABreakthroughCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABreakthroughCharacter();

	// First-person camera attached to the capsule, positioned at eye height (~1.7m).
	// Player sees the world through this camera; the SkeletalMesh is hidden from the
	// owning player (OwnerNoSee) so it doesn't occlude the view, but other players
	// (and the defender AI pawn) still see the mannequin body.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FirstPersonCamera;

	// Team this character belongs to: 0 = attackers, 1 = defenders.
	// Replicated so clients know each pawn's team for capture logic + visuals.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Battle")
	int32 Team = 0;

	// 保留 UPROPERTY 兼容 BP_BreakthroughCharacter 蓝图变量引用（不实际使用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultIMC = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* LookAction = nullptr;

	// ===== Defender AI patrol (C++ simple patrol, no NavMesh/BT needed) =====
	// When true, this character wanders inside PatrolRadius around PatrolCenter.
	// Set automatically in BeginPlay for non-player defender pawns (Team == 1).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	bool bIsPatrolling = false;

	// Centre of the patrol area (defaults to spawn location; GameMode sets it to the
	// defender SpawnHub position so the AI stays near its base).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	FVector PatrolCenter = FVector::ZeroVector;

	// Half-extent of the patrol box around PatrolCenter (cm). Default 500 = 10m × 10m.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	float PatrolRadius = 500.f;

	// ===== Player sprint (Shift to run, FPS-style) =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float NormalSpeed = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SprintSpeed = 800.f;

	// ===== V2 outer forbidden-zone / elimination =====
	// Replicated countdown shown only to the locally controlled player when outside the safe battlefield.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ForbiddenTimeRemaining, Category = "V2|ForbiddenZone")
	float ForbiddenTimeRemaining = 10.f;

	// True while this pawn is temporarily eliminated by the V2 outer forbidden zone.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Eliminated, Category = "V2|ForbiddenZone")
	bool bIsEliminated = false;

	// Called by AForbiddenZone overlap strips on the authoritative server.
	void SetForbiddenZoneOverlap(bool bEntering, const FString& SourceZone);

	// V2 config values supplied by GameMode / DataTable.
	void ConfigureForbiddenZone(float InCountdownSeconds, float InRespawnDelaySeconds, float InSafeHalfExtentX, float InSafeHalfExtentY);

	// Conquest round reset: GameMode teleports every human and defender AI back to
	// the matching base and clears any forbidden-zone death/timer state. Authority-only.
	void ResetForNewRound(const FVector& SpawnLocation, const FRotator& SpawnRotation);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// Replication: expose Team to clients.
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// 旧版 Input Axis 回调
	void MoveForward(float Value);
	void MoveRight(float Value);
	void LookUp(float Value);
	void Turn(float Value);

	// Sprint (Shift): raise/lower MaxWalkSpeed.
	void StartSprint();
	void StopSprint();

	// ===== Patrol internals =====
	// Current patrol destination; when reached (or after PatrolWaitTime), pick a new one.
	FVector PatrolTarget = FVector::ZeroVector;
	// Cooldown timer before picking the next patrol target (seconds).
	float PatrolWaitTimer = 0.f;
	// Cached idle/walk/jump AnimSequence references so we don't LoadObject every tick.
	class UAnimSequence* IdleAnimAsset = nullptr;
	class UAnimSequence* WalkAnimAsset = nullptr;
	class UAnimSequence* JumpAnimAsset = nullptr;
	bool bPatrolAnimIsWalk = false; // tracks which anim is currently playing
	bool bAnimIsJump = false; // tracks whether the jump anim is currently playing

	// V2 forbidden-zone implementation (server authoritative; state replicates to clients).
	UFUNCTION()
	void OnRep_ForbiddenTimeRemaining();

	UFUNCTION()
	void OnRep_Eliminated();

	void UpdateForbiddenZone(float DeltaTime);
	// Local HUD hint for the current gameplay region (replaces removed 3D labels).
	void UpdateAreaHint();
	void EliminateInForbiddenZone();
	void RespawnFromForbiddenZone();

	int32 ForbiddenZoneOverlapCount = 0;
	float ForbiddenCountdownDuration = 10.f;
	float ForbiddenRespawnDelay = 3.f;
	// V2 safe rectangle half extents (cm): everything outside this is forbidden,
	// all the way to the map edge. Position-based check, not trigger-strip overlap.
	float SafeHalfExtentX = 4500.f;
	float SafeHalfExtentY = 3600.f;
	bool bWasInForbiddenZone = false;
	int32 LastForbiddenDisplaySecond = -1;
	FString LastShownArea;
	FTimerHandle ForbiddenRespawnTimer;

	// Pick a new random patrol target inside the box around PatrolCenter.
	void PickNewPatrolTarget();
	// Play walk anim if moving, idle anim if not.
	void UpdatePatrolAnim(bool bMoving);
};
