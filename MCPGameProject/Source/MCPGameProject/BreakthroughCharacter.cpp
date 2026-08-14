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
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "SpawnAreaHub.h"
#include "BattleAreaSpline.h"
#include "Components/SplineComponent.h"

void ABreakthroughCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABreakthroughCharacter, Team);
	DOREPLIFETIME(ABreakthroughCharacter, ForbiddenTimeRemaining);
	DOREPLIFETIME(ABreakthroughCharacter, bIsEliminated);
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

	// FPS-standard: the whole pawn (capsule + mesh) yaws with the controller (mouse),
	// so other players see this body facing wherever the owner is looking. Without this,
	// only the camera turns and the body keeps its spawn facing (looks sideways/frozen
	// to remote clients). The patrol AI has no controller — its Tick SetActorRotation
	// still drives its facing, unaffected by this flag.
	bUseControllerRotationYaw = true;

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

	// Align the mannequin with the capsule once, in the constructor. The UE4
	// Mannequin's reference pose faces +Y, so the standard ThirdPerson-template
	// alignment is a -90° relative yaw, making the body's front match the capsule's
	// forward (+X). From then on the mesh simply follows the pawn's rotation
	// (controller yaw for players, SetActorRotation for the patrol AI).
	// (An earlier 180° attempt assumed the pose faces -X — that left a 90° offset
	// between where the owner looks and where the body points.)
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

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
		JumpAnimAsset = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/AnimStarterPack/Jump_From_Stand"));
		UE_LOG(LogTemp, Warning, TEXT("Anim load: Idle=%s Walk=%s Jump=%s"),
			IdleAnimAsset ? TEXT("OK") : TEXT("NULL"),
			WalkAnimAsset ? TEXT("OK") : TEXT("NULL"),
			JumpAnimAsset ? TEXT("OK") : TEXT("NULL"));
		// Start with idle loop.
		if (IdleAnimAsset)
		{
			GetMesh()->PlayAnimation(IdleAnimAsset, true);
		}
	}

	// NOTE: the old "rotate mesh to face level center" BeginPlay hack was removed.
	// The mesh is now aligned to the capsule in the constructor (180° relative yaw) and
	// the pawn rotates via bUseControllerRotationYaw (players) / Tick SetActorRotation
	// (patrol AI), so the body always faces the right way dynamically.

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

void ABreakthroughCharacter::ResetForNewRound(const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	if (!HasAuthority())
	{
		return;
	}

	// Cancel a pending out-of-bounds respawn so it cannot overwrite the round reset
	// a few seconds later. Then restore this pawn to a fresh, controllable state.
	GetWorldTimerManager().ClearTimer(ForbiddenRespawnTimer);
	SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
	bIsEliminated = false;
	bWasInForbiddenZone = false;
	ForbiddenZoneOverlapCount = 0;
	ForbiddenTimeRemaining = ForbiddenCountdownDuration;
	LastForbiddenDisplaySecond = -1;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Walking);
	}

	// The defender AI returns to its own base/patrol area too; humans simply remain
	// player-controlled and receive their normal input immediately after the teleport.
	if (Team == 1 && !IsPlayerControlled())
	{
		PatrolCenter = SpawnLocation;
		PatrolTarget = FVector::ZeroVector;
		PatrolWaitTimer = 0.5f;
		bIsPatrolling = true;
	}
	ForceNetUpdate();
}

void ABreakthroughCharacter::SetForbiddenZoneOverlap(bool bEntering, const FString& SourceZone)
{
	// Visual border strips only: keep an overlap count for logging, but NEVER touch
	// the countdown here. The countdown is driven exclusively by the position-based
	// check in UpdateForbiddenZone (Spline polygon / safe rectangle). Resetting the
	// timer on strip exit caused the "crossing the boundary resets to 10s" bug.
	if (!HasAuthority() || bIsEliminated)
	{
		return;
	}
	ForbiddenZoneOverlapCount = FMath::Max(0, ForbiddenZoneOverlapCount + (bEntering ? 1 : -1));
}

void ABreakthroughCharacter::ConfigureForbiddenZone(float InCountdownSeconds, float InRespawnDelaySeconds, float InSafeHalfExtentX, float InSafeHalfExtentY)
{
	if (!HasAuthority())
	{
		return;
	}
	ForbiddenCountdownDuration = FMath::Max(1.f, InCountdownSeconds);
	ForbiddenRespawnDelay = FMath::Max(0.5f, InRespawnDelaySeconds);
	SafeHalfExtentX = FMath::Max(100.f, InSafeHalfExtentX);
	SafeHalfExtentY = FMath::Max(100.f, InSafeHalfExtentY);
	ForbiddenTimeRemaining = ForbiddenCountdownDuration;
}

void ABreakthroughCharacter::OnRep_ForbiddenTimeRemaining()
{
	if (!IsLocallyControlled() || bIsEliminated || ForbiddenTimeRemaining >= ForbiddenCountdownDuration)
	{
		return;
	}

	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(ForbiddenTimeRemaining));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9001, 1.1f, FColor(255, 80, 60),
			FString::Printf(TEXT("WARNING: FORBIDDEN ZONE - RETURN IN %d"), Seconds), true, FVector2D(1.6f, 1.6f));
	}
}

void ABreakthroughCharacter::OnRep_Eliminated()
{
	SetActorHiddenInGame(bIsEliminated);
	SetActorEnableCollision(!bIsEliminated);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (bIsEliminated)
		{
			Movement->DisableMovement();
		}
		else
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}

	if (bIsEliminated && IsLocallyControlled() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage((uint64)-1, 3.f, FColor::Red,
			TEXT(">>> ELIMINATED: LEFT THE BATTLEFIELD <<<"), true, FVector2D(1.8f, 1.8f));
	}
}

void ABreakthroughCharacter::UpdateForbiddenZone(float DeltaTime)
{
	if (!HasAuthority() || bIsEliminated)
	{
		return;
	}

	// Position-based forbidden check (not trigger-strip overlap): everything outside
	// the safe rectangle (own base / combat area / anchor zone) is forbidden, all the
	// way to the map edge. Walking deeper outward keeps counting down; only returning
	// inside the safe rectangle cancels it.
	const FVector Loc = GetActorLocation();
	// If one or more V2 safe spline polygons exist, they become the authoritative
	// boundary. This lets designers reshape the rectangle in-editor (concave/custom
	// battlefield outlines) without changing the character code. Fall back to the
	// DataTable rectangle during early startup or in maps without a spline actor.
	bool bHasSafeSpline = false;
	bool bInsideSafeSpline = false;
	for (TActorIterator<ABattleAreaSpline> It(GetWorld()); It; ++It)
	{
		if (It->bSafeArea)
		{
			bHasSafeSpline = true;
			bInsideSafeSpline |= It->IsPointInside(Loc);
		}
	}
	const bool bInForbidden = bHasSafeSpline
		? !bInsideSafeSpline
		: (FMath::Abs(Loc.X) > SafeHalfExtentX) || (FMath::Abs(Loc.Y) > SafeHalfExtentY);

	if (!bInForbidden)
	{
		if (bWasInForbiddenZone)
		{
			bWasInForbiddenZone = false;
			ForbiddenTimeRemaining = ForbiddenCountdownDuration;
			LastForbiddenDisplaySecond = -1;
			UE_LOG(LogTemp, Log, TEXT("V2 ForbiddenZone: %s returned to safe area"), *GetName());
		}
		return;
	}

	if (!bWasInForbiddenZone)
	{
		bWasInForbiddenZone = true;
		UE_LOG(LogTemp, Warning, TEXT("V2 ForbiddenZone: %s left the safe area at %s (countdown %.0fs)"),
			*GetName(), *Loc.ToString(), ForbiddenTimeRemaining);
	}

	ForbiddenTimeRemaining = FMath::Max(0.f, ForbiddenTimeRemaining - DeltaTime);
	const int32 DisplaySecond = FMath::CeilToInt(ForbiddenTimeRemaining);
	if (DisplaySecond != LastForbiddenDisplaySecond)
	{
		LastForbiddenDisplaySecond = DisplaySecond;
		UE_LOG(LogTemp, Warning, TEXT("V2 ForbiddenZone: %s must return in %d seconds"), *GetName(), DisplaySecond);
		// Host player's local pawn does not receive an OnRep callback from its own server write.
		OnRep_ForbiddenTimeRemaining();
	}

	if (ForbiddenTimeRemaining <= 0.f)
	{
		EliminateInForbiddenZone();
	}
}

void ABreakthroughCharacter::UpdateAreaHint()
{
	// Local-player HUD hint only (cosmetic, no replication): shows which V2 gameplay
	// region the player currently stands in, replacing the removed 3D floating labels.
	if (!IsLocallyControlled() || !GEngine)
	{
		return;
	}

	const FVector Loc = GetActorLocation();
	FString CurrentArea;
	FColor AreaColor = FColor::White;

	// First containing region wins on ties via smallest diagonal (a sector outline
	// sits inside the big combat rectangle, so size ordering keeps the hint specific).
	float BestDiagonal = FLT_MAX;
	for (TActorIterator<ABattleAreaSpline> It(GetWorld()); It; ++It)
	{
		ABattleAreaSpline* Area = *It;
		if (!Area || Area->bSafeArea || !Area->IsPointInside(Loc))
		{
			continue;
		}
		const FVector P0 = Area->BoundarySpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
		const FVector P2 = Area->BoundarySpline->GetLocationAtSplinePoint(2, ESplineCoordinateSpace::World);
		const float Diagonal = FVector::Dist2D(P0, P2);
		if (Diagonal < BestDiagonal)
		{
			BestDiagonal = Diagonal;
			CurrentArea = Area->AreaName;
			AreaColor = Area->BoundaryColor.ToFColor(true);
		}
	}

	if (CurrentArea.IsEmpty())
	{
		// Inside the safe battlefield but outside every named region.
		CurrentArea = TEXT("BATTLEFIELD");
	}

	if (CurrentArea != LastShownArea)
	{
		LastShownArea = CurrentArea;
		GEngine->AddOnScreenDebugMessage((uint64)-2, 3.f, AreaColor,
			FString::Printf(TEXT("--- %s ---"), *CurrentArea), true, FVector2D(1.2f, 1.2f));
	}
}

void ABreakthroughCharacter::EliminateInForbiddenZone()
{
	if (!HasAuthority() || bIsEliminated)
	{
		return;
	}

	bIsEliminated = true;
	bIsPatrolling = false;
	ForbiddenZoneOverlapCount = 0;
	ForbiddenTimeRemaining = 0.f;
	OnRep_Eliminated(); // host immediate feedback; remote clients receive replication.
	ForceNetUpdate();

	UE_LOG(LogTemp, Warning, TEXT("V2 ELIMINATION: %s left the battlefield; respawning in %.1fs"),
		*GetName(), ForbiddenRespawnDelay);
	GetWorldTimerManager().SetTimer(ForbiddenRespawnTimer, this, &ABreakthroughCharacter::RespawnFromForbiddenZone,
		ForbiddenRespawnDelay, false);
}

void ABreakthroughCharacter::RespawnFromForbiddenZone()
{
	if (!HasAuthority())
	{
		return;
	}

	FVector RespawnLocation = GetActorLocation();
	FRotator RespawnRotation = FRotator::ZeroRotator;
	for (TActorIterator<ASpawnAreaHub> It(GetWorld()); It; ++It)
	{
		ASpawnAreaHub* Hub = *It;
		if (Hub && Hub->Team == Team)
		{
			if (AActor* SpawnPoint = Hub->GetRandomSpawnPoint())
			{
				RespawnLocation = SpawnPoint->GetActorLocation();
				RespawnRotation = SpawnPoint->GetActorRotation();
			}
			else
			{
				RespawnLocation = Hub->GetActorLocation() + FVector(0.f, 0.f, 100.f);
			}
			break;
		}
	}

	SetActorLocationAndRotation(RespawnLocation, RespawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
	bIsEliminated = false;
	bWasInForbiddenZone = false;
	ForbiddenTimeRemaining = ForbiddenCountdownDuration;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	if (Team == 1 && !IsPlayerControlled())
	{
		bIsPatrolling = true;
		PatrolWaitTimer = 0.5f;
	}
	ForceNetUpdate();
	UE_LOG(LogTemp, Warning, TEXT("V2 RESPAWN: %s respawned at %s"), *GetName(), *RespawnLocation.ToString());
}

void ABreakthroughCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateForbiddenZone(DeltaTime);
	UpdateAreaHint();
	if (bIsEliminated)
	{
		return;
	}

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

	// Airborne takes priority: play the jump anim once (not looping) while falling.
	const bool bInAir = GetCharacterMovement() && GetCharacterMovement()->IsFalling();
	if (bInAir && !bAnimIsJump && JumpAnimAsset)
	{
		GetMesh()->PlayAnimation(JumpAnimAsset, false /*no loop*/);
		bAnimIsJump = true;
		bPatrolAnimIsWalk = false;
		return;
	}
	// Landed: resume locomotion anim on the next evaluation.
	if (!bInAir && bAnimIsJump)
	{
		bAnimIsJump = false;
		bPatrolAnimIsWalk = !bMoving; // force a locomotion refresh below
	}
	if (bAnimIsJump)
	{
		return; // still airborne, keep the jump anim playing
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

	// Jump: ACharacter has built-in Jump/StopJumping (uses CharacterMovement JumpZVelocity).
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
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
