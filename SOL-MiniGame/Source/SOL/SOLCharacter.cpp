// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLCharacter.h"
#include "SOL.h"
#include "SOLBackpackComponent.h"
#include "SOLHealthComponent.h"
#include "SOLItemPickup.h"
#include "SOLPlayerController.h"
#include "SOLPlayerState.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Animation/AnimSequence.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ASOLCharacter::ASOLCharacter()
{
	// Animation is driven from Tick (see UpdateLocomotionAnimation): the
	// AnimStarterPack AnimBlueprint casts to its own Ue4ASP_Character class,
	// which this pawn is not, so its state machine would never leave idle.
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	BaseEyeHeight = 64.f;
	bUseControllerRotationYaw = true;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// Third-person body: SK_Mannequin from AnimStarterPack (same model as
	// the battlefield project). Sits inside the capsule; OwnerNoSee keeps
	// it out of the owning player's first-person view, but every other
	// client sees a recognizable human skeleton.
	BodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -96.f)); // capsule centre → feet at 0
	BodyMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetOwnerNoSee(true);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinMesh(TEXT("SkeletalMesh'/Game/AnimStarterPack/UE4_Mannequin/Mesh/SK_Mannequin.SK_Mannequin'"));
	if (MannequinMesh.Succeeded())
	{
		BodyMesh->SetSkeletalMesh(MannequinMesh.Object);
	}

	// Animation. Without an AnimInstance the mesh just slides around in its
	// bind pose — which is exactly how it looked before (user feedback
	// 2026-09-01: "平移都是模型的平移没用上奔跑的动画"). AnimStarterPack does ship
	// an AnimBlueprint, but its graph casts to Ue4ASP_Character (its own
	// blueprint pawn class); that cast fails for this pawn, so the state
	// machine never leaves idle. Explicit clips driven from Tick are both
	// predictable and let us tie the animation to encumbrance.
	BodyMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	// Backpack contents replicate (owner-only) — the component opts in
	// itself (SetIsReplicatedByDefault in its constructor); the server is
	// the single writer.
	Backpack = CreateDefaultSubobject<USOLBackpackComponent>(TEXT("Backpack"));

	// Health/damage. Same component on players and scavengers, so a bullet
	// does not care what it hit.
	HealthComp = CreateDefaultSubobject<USOLHealthComponent>(TEXT("Health"));

	// ACharacter already replicates movement (ServerMove / ClientAdjustPosition
	// via the CharacterMovementComponent) — nothing to opt into manually.
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
}

void ASOLCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Load the locomotion clips once. AnimStarterPack has no plain
	// "Walk_Fwd_Rifle_Hip" — only the _Ironsights variant — so that is what the
	// walk band uses (the upper body pose differs slightly, unnoticeable at the
	// distance other players are seen from).
	IdleAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/AnimStarterPack/Idle_Rifle_Hip"));
	WalkAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/AnimStarterPack/Walk_Fwd_Rifle_Ironsights"));
	JogAnim  = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/AnimStarterPack/Jog_Fwd_Rifle"));
	DeathAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/AnimStarterPack/Death_1"));

	UE_LOG(LogSOL, Log, TEXT("Anim load: idle=%s walk=%s jog=%s death=%s"),
		IdleAnim ? TEXT("OK") : TEXT("NULL"),
		WalkAnim ? TEXT("OK") : TEXT("NULL"),
		JogAnim  ? TEXT("OK") : TEXT("NULL"),
		DeathAnim ? TEXT("OK") : TEXT("NULL"));

	CurrentAmmo = MagazineSize;

	// Death is decided on the server; the delegate only ever fires there.
	if (HealthComp)
	{
		HealthComp->OnDeath.BindUObject(this, &ASOLCharacter::HandleDeath);
	}

	UpdateLocomotionAnimation();
}

void ASOLCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Runs on every instance (server, owning client, simulated proxies) because
	// velocity is replicated by the movement component — each side derives the
	// same clip from the same speed with no extra networking.
	UpdateLocomotionAnimation();
}

bool ASOLCharacter::IsDead() const
{
	return HealthComp && HealthComp->IsDead();
}

float ASOLCharacter::GetHealthRatio() const
{
	return HealthComp ? HealthComp->GetHealthRatio() : 0.f;
}

void ASOLCharacter::GetAimRay(FVector& OutStart, FVector& OutDir) const
{
	// Players shoot from the camera, so the shot goes exactly where the
	// crosshair is. (The scavenger overrides this — it has no camera.)
	if (FirstPersonCamera)
	{
		OutStart = FirstPersonCamera->GetComponentLocation();
		OutDir = FirstPersonCamera->GetForwardVector();
		return;
	}
	OutStart = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
	OutDir = GetActorForwardVector();
}

void ASOLCharacter::TryFire()
{
	if (IsDead() || bReloading)
	{
		return;
	}

	// Local rate limit: keeps a held trigger from spamming RPCs the server
	// would only throw away. The server enforces the same interval itself.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastFireLocalTime < FireInterval)
	{
		return;
	}
	LastFireLocalTime = Now;

	if (CurrentAmmo <= 0)
	{
		// Empty: start reloading instead of doing nothing, so an empty mag
		// never feels like a broken button.
		TryReload();
		return;
	}

	FVector Start, Dir;
	GetAimRay(Start, Dir);
	ServerFireShot(Start, Dir.GetSafeNormal());
}

void ASOLCharacter::TryReload()
{
	if (IsDead() || bReloading || CurrentAmmo >= MagazineSize)
	{
		return;
	}
	ServerReload();
}

void ASOLCharacter::ServerFireShot_Implementation(FVector_NetQuantize TraceStart, FVector_NetQuantizeNormal TraceDir)
{
	UWorld* World = GetWorld();
	if (!World || IsDead() || bReloading || CurrentAmmo <= 0)
	{
		return;
	}

	// Server-side rate limit.
	const float Now = World->GetTimeSeconds();
	if (Now - LastFireServerTime < FireInterval * 0.9f) // 10% slack for jitter
	{
		return;
	}
	LastFireServerTime = Now;

	// Sanity-check the claimed origin against the server's own copy of this
	// pawn. A client may aim wherever it likes, but it cannot shoot from a
	// different place than where the server thinks it is standing.
	const FVector ServerEye = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
	if (FVector::DistSquared(ServerEye, TraceStart) > FMath::Square(220.f))
	{
		UE_LOG(LogSOL, Warning, TEXT("ServerFireShot REJECTED: %s origin off by %.0fcm"),
			*GetName(), FVector::Dist(ServerEye, TraceStart));
		return;
	}

	FVector Dir = FVector(TraceDir);
	if (!Dir.Normalize())
	{
		return;
	}

	--CurrentAmmo;

	const FVector End = FVector(TraceStart) + Dir * WeaponRange;

	// ECC_Pawn, not ECC_Visibility: the character capsule's "Pawn" collision
	// profile ignores the Visibility channel, so a Visibility trace would fly
	// straight through players while still being blocked by walls — bullets
	// that hit scenery but never people. Pawn is blocked by both.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SOLShot), false, this);
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, FVector(TraceStart), End, ECC_Pawn, Params);
	const FVector Impact = bHit ? Hit.ImpactPoint : End;

	MulticastShotFired(FVector(TraceStart), Impact);

	if (!bHit)
	{
		return;
	}

	AActor* HitActor = Hit.GetActor();
	ASOLCharacter* Victim = Cast<ASOLCharacter>(HitActor);
	if (!Victim || Victim->IsDead())
	{
		return; // scenery, or a corpse
	}

	const float HealthBefore = Victim->HealthComp ? Victim->HealthComp->Health : 0.f;
	UGameplayStatics::ApplyPointDamage(Victim, WeaponDamage, Dir, Hit,
		GetController(), this, UDamageType::StaticClass());
	const bool bKilled = Victim->IsDead();

	UE_LOG(LogSOL, Log, TEXT("SHOT: %s hit %s at %.0fcm (%.0f -> %.0f hp)%s"),
		*GetName(), *Victim->GetName(), Hit.Distance, HealthBefore,
		Victim->HealthComp ? Victim->HealthComp->Health : 0.f,
		bKilled ? TEXT(" [KILL]") : TEXT(""));

	// Bounty is only known at the moment of death (it is the loot the victim
	// was carrying), so read it before the backpack gets spilled — the death
	// handler runs inside ApplyPointDamage above, which means by now the
	// backpack is already empty. Credit the value recorded on the killer's
	// player state instead.
	int32 Bounty = 0;
	if (bKilled)
	{
		if (const ASOLPlayerState* MyPS = GetPlayerState<ASOLPlayerState>())
		{
			Bounty = MyPS->LastKillBounty;
		}
	}
	// Only worth sending to a human: a scavenger has no owning connection, so
	// the RPC would be dropped by the net driver anyway. Skipping it keeps the
	// AI's shots off the networking path entirely.
	if (Cast<APlayerController>(GetController()))
	{
		ClientHitFeedback(bKilled, bKilled ? Victim->GetHumanReadableName() : FString(), Bounty);
	}
}

bool ASOLCharacter::ServerFireShot_Validate(FVector_NetQuantize TraceStart, FVector_NetQuantizeNormal TraceDir)
{
	// Range and state checks live in the implementation so that a legitimate
	// race (dying mid-trigger) degrades into a no-op instead of kicking the
	// player off the server.
	return true;
}

void ASOLCharacter::ServerReload_Implementation()
{
	if (IsDead() || bReloading || CurrentAmmo >= MagazineSize)
	{
		return;
	}
	bReloading = true;
	UE_LOG(LogSOL, Verbose, TEXT("Reload start: %s (%d/%d)"), *GetName(), CurrentAmmo, MagazineSize);

	GetWorldTimerManager().SetTimer(ReloadTimer, this, &ASOLCharacter::FinishReload, ReloadSeconds, false);
}

void ASOLCharacter::FinishReload()
{
	if (!HasAuthority())
	{
		return;
	}
	CurrentAmmo = MagazineSize;
	bReloading = false;
}

void ASOLCharacter::MulticastShotFired_Implementation(FVector_NetQuantize From, FVector_NetQuantize To)
{
	// Zero-asset tracer: a thin line that lives for a tenth of a second. Real
	// projects use a particle system + muzzle flash; for a demo this is enough
	// to see who is shooting where, and it costs no content pipeline at all.
	if (UWorld* World = GetWorld())
	{
		DrawDebugLine(World, FVector(From), FVector(To), FColor(255, 220, 120), false, 0.12f, 0, 1.5f);
	}
}

void ASOLCharacter::ClientHitFeedback_Implementation(bool bKilled, const FString& VictimName, int32 Bounty)
{
	// Routed through the player controller so the HUD stays the only thing
	// that knows how feedback looks.
	if (ASOLPlayerController* PC = Cast<ASOLPlayerController>(GetController()))
	{
		PC->OnLocalHitConfirmed(bKilled, VictimName, Bounty);
	}
}

void ASOLCharacter::OnRep_Ammo()
{
	// Presentation only — the HUD reads CurrentAmmo every frame.
}

void ASOLCharacter::SpillBackpack()
{
	UWorld* World = GetWorld();
	if (!World || !Backpack || !HasAuthority())
	{
		return;
	}

	const int32 Count = Backpack->Items.Num();
	if (Count == 0)
	{
		return;
	}

	// Spread the stacks on a small ring so they are individually pickable
	// instead of stacking into one unclickable pile.
	const FVector Base = GetActorLocation();
	for (int32 Idx = 0; Idx < Count; ++Idx)
	{
		const float Angle = (2.f * PI * Idx) / Count;
		const FVector Offset(FMath::Cos(Angle) * 110.f, FMath::Sin(Angle) * 110.f, 0.f);

		// Grounded spawn: the dying pawn's own capsule is still solid at this
		// point, so spawning raw would shove the pickup on top of it and the
		// loot would hover mid-air.
		ASOLItemPickup::SpawnGrounded(World, Base + Offset, Backpack->Items[Idx]);
	}

	UE_LOG(LogSOL, Log, TEXT("SPILL: %s dropped %d stack(s) worth %d"),
		*GetName(), Count, Backpack->CurrentValue());

	// Clearing through the component keeps the encumbrance recalculation in
	// one place (Items changing must always re-apply the walk speed).
	Backpack->ClearAll();
}

void ASOLCharacter::HandleDeath(AActor* Victim, AController* Killer)
{
	if (!HasAuthority())
	{
		return;
	}

	// Bounty = what the victim was carrying. Read it before the spill, and
	// stash it on the killer's player state so the kill feed can quote it.
	const int32 CarriedValue = Backpack ? Backpack->CurrentValue() : 0;
	if (ASOLPlayerState* KillerPS = Killer ? Killer->GetPlayerState<ASOLPlayerState>() : nullptr)
	{
		KillerPS->RegisterKill(CarriedValue);
	}
	if (ASOLPlayerState* MyPS = GetPlayerState<ASOLPlayerState>())
	{
		MyPS->RegisterDeath();
	}

	SpillBackpack();

	// Stop moving and turn off collision so the corpse neither blocks a
	// doorway nor absorbs further bullets.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Death pose: one-shot clip, no looping. Runs on the server and replicates
	// nothing — every client's own Tick sees velocity 0 and would otherwise
	// pick idle, so the clip is set locally on each side via the same call in
	// UpdateLocomotionAnimation's dead-check.
	if (BodyMesh && DeathAnim)
	{
		BodyMesh->PlayAnimation(DeathAnim, false);
		CurrentAnim = DeathAnim;
	}

	UE_LOG(LogSOL, Log, TEXT("HandleDeath: %s (carried %d) killed by %s"),
		*GetName(), CarriedValue, Killer ? *Killer->GetName() : TEXT("<world>"));

	// Player respawn is the GameMode's business (it owns the player starts and
	// the "already extracted, no respawn" rule). Scavengers override this and
	// simply remove themselves.
	if (ASOLPlayerController* PC = Cast<ASOLPlayerController>(GetController()))
	{
		PC->NotifyDeath(Killer);
	}
}

void ASOLCharacter::UpdateLocomotionAnimation()
{
	if (!BodyMesh)
	{
		return;
	}

	// A corpse keeps its death pose: without this the clip below would drag
	// it back to idle the moment the body stops sliding.
	if (IsDead())
	{
		if (DeathAnim && CurrentAnim != DeathAnim)
		{
			BodyMesh->PlayAnimation(DeathAnim, false);
			CurrentAnim = DeathAnim;
			BodyMesh->SetPlayRate(1.f);
		}
		return;
	}

	// Horizontal speed only: falling should not look like sprinting.
	const float Speed = GetVelocity().Size2D();

	// Band thresholds sit inside the encumbrance range (330 满载 .. 600 轻装),
	// so an overloaded player visibly walks while a light one jogs — the load
	// is readable from the silhouette alone.
	UAnimSequence* Desired = IdleAnim;
	float PlayRate = 1.f;
	if (Speed > 340.f)
	{
		Desired = JogAnim;
		// Jog_Fwd_Rifle is authored around ~600 u/s; scaling the rate keeps the
		// feet roughly in sync with the ground instead of skating.
		PlayRate = FMath::Clamp(Speed / 600.f, 0.75f, 1.25f);
	}
	else if (Speed > 20.f)
	{
		Desired = WalkAnim;
		PlayRate = FMath::Clamp(Speed / 200.f, 0.7f, 1.6f);
	}

	if (!Desired)
	{
		return; // asset missing; leave the mesh in its bind pose
	}

	// Only restart on an actual clip change — calling PlayAnimation every frame
	// would reset the clip to frame 0 and freeze the character mid-stride.
	if (Desired != CurrentAnim)
	{
		BodyMesh->PlayAnimation(Desired, true);
		CurrentAnim = Desired;
	}
	BodyMesh->SetPlayRate(PlayRate);
}

void ASOLCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Legacy input mappings declared in Config/DefaultInput.ini — zero asset
	// setup, unlike EnhancedInput which needs IA assets created in-editor.
	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ASOLCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ASOLCharacter::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
}

void ASOLCharacter::MoveForward(float Value)
{
	if (Value != 0.f && !IsDead())
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void ASOLCharacter::MoveRight(float Value)
{
	if (Value != 0.f && !IsDead())
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void ASOLCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Ammo is private to its owner; the reload flag is public because other
	// players seeing "he is reloading" is legitimate information (and it is
	// what the third-person animation would key off in a fuller version).
	DOREPLIFETIME_CONDITION(ASOLCharacter, CurrentAmmo, COND_OwnerOnly);
	DOREPLIFETIME(ASOLCharacter, bReloading);
}
