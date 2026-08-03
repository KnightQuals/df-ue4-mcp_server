// Copyright Epic Games, Inc. All Rights Reserved.

#include "BreakthroughCharacter.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

void ABreakthroughCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABreakthroughCharacter, Team);
}

// Sets default values
ABreakthroughCharacter::ABreakthroughCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Enable network replication so 2-player PIE (Listen Server) shows each player's
	// pawn + movement on the other client. ACharacter already replicates movement by
	// default, but we set these explicitly for clarity and to be safe.
	bReplicates = true;
	SetReplicateMovement(true);

	// Default player walk speed (NormalSpeed). Shift raises it to SprintSpeed.
	// The defender AI uses SetActorLocation for patrol (not this speed), so this is
	// effectively the player's normal walking pace.
	GetCharacterMovement()->MaxWalkSpeed = NormalSpeed;

	// First-person camera attached to the capsule, positioned at eye height.
	// The owning player views the world through this camera; the SkeletalMesh is
	// hidden from the owner (OwnerNoSee) so it doesn't block the view, but other
	// pawns (defender AI) still see the mannequin body.
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetRootComponent());
	// Eye height ~= capsule half-height (88) + a bit, so the camera sits at the head.
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->SetRelativeRotation(FRotator::ZeroRotator);
	// Let PawnControlPitch/Yaw drive the camera (DefaultPawn-style).
	FirstPersonCamera->bUsePawnControlRotation = true;

	// Load the UE4 Mannequin (Animation Starter Pack) and assign it to the engine's
	// built-in SkeletalMeshComponent (GetMesh).
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinMeshFinder(
		TEXT("/Game/AnimStarterPack/UE4_Mannequin/Mesh/SK_Mannequin"));
	if (MannequinMeshFinder.Succeeded())
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			MeshComp->SetSkeletalMesh(MannequinMeshFinder.Object);
			// Drop the mesh so its feet sit at the capsule's base.
			MeshComp->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
			MeshComp->SetVisibility(true);
			// Hide the mesh from the owning player so it doesn't occlude the camera.
			// Other pawns (defender AI, multiplayer clients) still see this body.
			MeshComp->SetOwnerNoSee(true);
		}
	}
}

// Called when the game starts or when spawned
void ABreakthroughCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Cache idle + walk anim assets (Animation Starter Pack has no AnimBlueprint,
	// so we drive them directly via PlayAnimation).
	if (GetMesh())
	{
		IdleAnimAsset = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/AnimStarterPack/Idle_Rifle_Hip"));
		// NOTE: AnimStarterPack has no "Walk_Fwd_Rifle_Hip" — only the _Ironsights variant
		// and Jog_Fwd_Rifle. Use Jog_Fwd_Rifle for a natural forward locomotion cycle.
		WalkAnimAsset = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/AnimStarterPack/Jog_Fwd_Rifle"));
		UE_LOG(LogTemp, Warning, TEXT("Anim load: Idle=%s Walk=%s"),
			IdleAnimAsset ? TEXT("OK") : TEXT("NULL"),
			WalkAnimAsset ? TEXT("OK") : TEXT("NULL"));
		// Start with idle loop.
		if (IdleAnimAsset)
		{
			GetMesh()->PlayAnimation(IdleAnimAsset, true);
		}
	}

	// Rotate the mesh (not the capsule) to face the level center so the mannequin
	// is visible face-on when the player approaches. UE4 Mannequin's reference pose
	// faces -X; we add 180° to flip it so the front of the body points TOWARD center.
	if (GetMesh())
	{
		FVector ToCenter = FVector(0.f, 0.f, 0.f) - GetActorLocation();
		ToCenter.Z = 0.f;
		if (!ToCenter.IsNearlyZero())
		{
			FRotator LookAtCenter = ToCenter.Rotation();
			GetMesh()->SetWorldRotation(FRotator(0.f, LookAtCenter.Yaw + 180.f, 0.f));
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Character spawned team=%d"), Team);

	// Start patrolling for non-player defender AI. The GameMode's PostLogin sets
	// PatrolCenter to the defender SpawnHub location; if it wasn't set, default to
	// the spawn location so the AI still wanders around where it spawned.
	if (Team == 1 && !IsPlayerControlled())
	{
		if (PatrolCenter.IsNearlyZero())
		{
			PatrolCenter = GetActorLocation();
		}
		bIsPatrolling = true;
		PatrolWaitTimer = 0.5f; // short pause before first move
		UE_LOG(LogTemp, Warning, TEXT("Defender AI patrol started, center=%s radius=%.0f"),
			*PatrolCenter.ToString(), PatrolRadius);
	}
}

void ABreakthroughCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsPatrolling)
	{
		// Player-controlled character (or any non-patrol pawn): drive Idle/Walk
		// animation from the actual movement speed so the body animates for other
		// players / third-person views. The owning player can't see their own mesh
		// (OwnerNoSee) but multiplayer clients and spectators see this.
		const float Speed = GetVelocity().Size2D();
		UpdatePatrolAnim(Speed > 10.f); // moving if horizontal speed > 10 cm/s
		return;
	}

	// Patrol MOVEMENT must run on the server only (authority). SetActorLocation on the
	// server replicates the new position to clients automatically. Clients still play
	// the animation locally based on replicated velocity below.
	if (!HasAuthority())
	{
		const float ClientSpeed = GetVelocity().Size2D();
		UpdatePatrolAnim(ClientSpeed > 10.f);
		return;
	}

	// If waiting between patrol points, count down then pick a new target.
	if (PatrolWaitTimer > 0.f)
	{
		PatrolWaitTimer -= DeltaTime;
		UpdatePatrolAnim(false);
		return;
	}

	// If we don't have a target yet, pick one.
	if (PatrolTarget.IsNearlyZero())
	{
		PickNewPatrolTarget();
	}

	// Move toward PatrolTarget.
	const FVector CurrentLoc = GetActorLocation();
	FVector ToTarget = PatrolTarget - CurrentLoc;
	ToTarget.Z = 0.f; // keep patrol on the horizontal plane
	const float Dist = ToTarget.Size();

	// Reached target (within 150cm) → pause 2-4s, then pick a new one.
	// Wider threshold suits the larger 24m patrol area (50cm was too precise).
	if (Dist < 150.f)
	{
		PatrolWaitTimer = 2.f + FMath::FRand() * 2.f; // 2~4s idle pause
		PatrolTarget = FVector::ZeroVector;
		UpdatePatrolAnim(false);
		return;
	}

	// Face the target and walk forward.
	const FRotator TargetRot = ToTarget.GetSafeNormal().Rotation();
	FRotator CurrentRot = GetActorRotation();
	CurrentRot.Pitch = 0.f;
	CurrentRot.Roll = 0.f;
	// Smoothly rotate toward the target.
	FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 5.f);
	SetActorRotation(NewRot);

	// Move forward toward the target using SetActorLocation with sweep=true.
	// This is the most robust option for a Controller-less spawned pawn: it moves
	// the capsule directly and still resolves collision (sweep), without relying on
	// CharacterMovementComponent.PerformMovement (which bails when no Controller
	// owns the pawn) or AddMovementInput (which needs a Controller to consume input).
	const FVector MoveDir = ToTarget.GetSafeNormal();
	const float Step = 300.f * DeltaTime; // 3 m/s jog — faster to cover the larger patrol area
	const FVector NewLoc = CurrentLoc + MoveDir * Step;
	FHitResult SweepHit;
	SetActorLocation(NewLoc, true /*bSweep*/, &SweepHit);

	// Diagnostic (throttled): confirm the patrol tick is moving the pawn.
	static float LogAccum = 0.f;
	LogAccum += DeltaTime;
	if (LogAccum >= 1.f)
	{
		LogAccum = 0.f;
		UE_LOG(LogTemp, Warning, TEXT("Patrol MOVE: loc=%s target=%s dist=%.0f step=%.1f"),
			*GetActorLocation().ToString(), *PatrolTarget.ToString(), Dist, Step);
	}

	UpdatePatrolAnim(true);
}

void ABreakthroughCharacter::PickNewPatrolTarget()
{
	// Random point inside the patrol box centred on PatrolCenter.
	const float X = PatrolCenter.X + (FMath::FRand() * 2.f - 1.f) * PatrolRadius;
	const float Y = PatrolCenter.Y + (FMath::FRand() * 2.f - 1.f) * PatrolRadius;
	PatrolTarget = FVector(X, Y, PatrolCenter.Z);
	UE_LOG(LogTemp, Log, TEXT("Patrol new target: %s"), *PatrolTarget.ToString());
}

void ABreakthroughCharacter::UpdatePatrolAnim(bool bMoving)
{
	if (!GetMesh())
	{
		return;
	}

	// Only switch the animation when the desired state actually changes — calling
	// PlayAnimation every tick would restart the anim and look glitchy.
	if (bMoving && !bPatrolAnimIsWalk && WalkAnimAsset)
	{
		GetMesh()->PlayAnimation(WalkAnimAsset, true);
		bPatrolAnimIsWalk = true;
	}
	else if (!bMoving && bPatrolAnimIsWalk && IdleAnimAsset)
	{
		GetMesh()->PlayAnimation(IdleAnimAsset, true);
		bPatrolAnimIsWalk = false;
	}
}

// Called to bind functionality to input（旧版 Input BindAxis，不依赖 Enhanced Input 资产）
void ABreakthroughCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!PlayerInputComponent) return;

	// 旧版 Axis 绑定（按键映射在 Config/DefaultInput.ini）
	PlayerInputComponent->BindAxis("MoveForward", this, &ABreakthroughCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ABreakthroughCharacter::MoveRight);
	PlayerInputComponent->BindAxis("LookUp", this, &ABreakthroughCharacter::LookUp);
	PlayerInputComponent->BindAxis("Turn", this, &ABreakthroughCharacter::Turn);

	// Sprint: hold Shift to run (Pressed = sprint, Released = walk).
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &ABreakthroughCharacter::StartSprint);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &ABreakthroughCharacter::StopSprint);
}

void ABreakthroughCharacter::StartSprint()
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	}
}

void ABreakthroughCharacter::StopSprint()
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = NormalSpeed;
	}
}

void ABreakthroughCharacter::MoveForward(float Value)
{
	if (Controller && Value != 0.0f)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(ForwardDirection, Value);
	}
}

void ABreakthroughCharacter::MoveRight(float Value)
{
	if (Controller && Value != 0.0f)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(RightDirection, Value);
	}
}

void ABreakthroughCharacter::LookUp(float Value)
{
	if (Value != 0.0f)
	{
		AddControllerPitchInput(Value);
	}
}

void ABreakthroughCharacter::Turn(float Value)
{
	if (Value != 0.0f)
	{
		AddControllerYawInput(Value);
	}
}
