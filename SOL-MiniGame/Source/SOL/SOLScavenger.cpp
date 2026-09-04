// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLScavenger.h"
#include "SOL.h"
#include "SOLBackpackComponent.h"
#include "SOLHealthComponent.h"
#include "SOLItemTypes.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ASOLScavenger::ASOLScavenger()
{
	// A plain AIController is enough: it gives the pawn a Controller (without
	// one, AddMovementInput on a Character silently does nothing) and a control
	// rotation to steer with. No behaviour tree, no blackboard, no NavMesh.
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Softer than a player: 80 hp means three player hits (34 dmg) kill, so a
	// firefight is decided quickly and ammo economy stays irrelevant.
	if (HealthComp)
	{
		HealthComp->MaxHealth = 80.f;
		HealthComp->Health = 80.f;
		// No regeneration for NPCs: a scavenger that healed up between
		// engagements would turn every fight into a war of attrition.
		HealthComp->RegenPerSecond = 0.f;
	}

	// Weaker gun, slower trigger, shorter reach than the player's — the
	// player is supposed to win a fair fight and lose a careless one.
	// Tuned down from 12 dmg / 0.85 s after the first smoke test: a single
	// scavenger killed a stationary player in 9 shots, and the map can put two
	// of them on you at once. 10 dmg / 1.05 s means one scavenger needs ~10 s
	// of uninterrupted fire, which is enough time to shoot back or leave.
	WeaponDamage = 10.f;
	FireInterval = 1.05f;
	WeaponRange = 4000.f;
	MagazineSize = 12;
	ReloadSeconds = 2.6f;

	// Slower than the player's 600 so disengaging is always an option.
	GetCharacterMovement()->MaxWalkSpeed = 380.f;

	// The AI owns its own facing (see FaceLocation): letting the controller
	// drive the yaw would fight the direct SetActorRotation, because
	// AAIController copies the pawn's rotation back into the control rotation
	// every frame.
	bUseControllerRotationYaw = false;

	// Its own body must be visible to everyone, including whoever is shooting
	// at it — the base class hides the mesh from its owner for the player's
	// first-person view, which for an unpossessed-by-a-human pawn would just
	// make it invisible on the listen host.
	if (BodyMesh)
	{
		BodyMesh->SetOwnerNoSee(false);
	}
}

void ASOLScavenger::BeginPlay()
{
	Super::BeginPlay();

	if (HomeLocation.IsNearlyZero())
	{
		HomeLocation = GetActorLocation();
	}
	PatrolPoint = HomeLocation;

	if (HasAuthority())
	{
		RollLoot();
		PickNewPatrolPoint();

		// The base class captured 600 as the encumbrance baseline from the
		// parent's constructor value; re-apply so the scavenger's own 380 is
		// what the load scales down from.
		if (Backpack)
		{
			Backpack->BaseWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
			Backpack->ApplyEncumbranceToOwner();
		}

		UE_LOG(LogSOL, Log, TEXT("Scavenger %s ready at %s (loot %d stack(s) worth %d)"),
			*GetName(), *HomeLocation.ToCompactString(),
			Backpack ? Backpack->Items.Num() : 0,
			Backpack ? Backpack->CurrentValue() : 0);
	}
}

void ASOLScavenger::RollLoot()
{
	if (!Backpack)
	{
		return;
	}

	// A small hand-rolled pool rather than a DataTable lookup: NPC loot is
	// deliberately mid-value (the good stuff is in containers), and keeping it
	// in code means spawning scavengers needs no extra asset provisioning.
	static const FSOLItemDef Pool[] = {
		{ TEXT("AMMO"),        TEXT("弹药"),     3.0f, 8.f,  60 },
		{ TEXT("MEDKIT"),      TEXT("医疗包"),   1.0f, 7.f,  80 },
		{ TEXT("COINS"),       TEXT("金币"),     2.0f, 6.f, 120 },
		{ TEXT("WATCH"),       TEXT("名表"),     0.3f, 3.f, 300 },
		{ TEXT("NIGHT_VISION"),TEXT("夜视仪"),   1.2f, 2.f, 550 },
	};
	const int32 PoolSize = UE_ARRAY_COUNT(Pool);

	float TotalWeight = 0.f;
	for (int32 i = 0; i < PoolSize; ++i)
	{
		TotalWeight += Pool[i].SpawnWeight;
	}

	for (int32 Draw = 0; Draw < LootStacks; ++Draw)
	{
		float Roll = FMath::FRandRange(0.f, TotalWeight);
		for (int32 i = 0; i < PoolSize; ++i)
		{
			Roll -= Pool[i].SpawnWeight;
			if (Roll <= 0.f)
			{
				FSOLItemInstance Inst;
				Inst.Def = Pool[i];
				Inst.Count = 1;
				Backpack->AddItem(Inst);
				break;
			}
		}
	}
}

void ASOLScavenger::Tick(float DeltaSeconds)
{
	// Base Tick drives the locomotion animation on every instance, including
	// clients (velocity is replicated), so a remote scavenger is animated
	// without any AI running there.
	Super::Tick(DeltaSeconds);

	// Everything below is a decision, and decisions belong to the server.
	if (!HasAuthority() || IsDead())
	{
		return;
	}

	SenseAccumulator += DeltaSeconds;
	if (SenseAccumulator >= 0.25f)
	{
		SenseAccumulator = 0.f;
		UpdatePerception();
	}

	switch (State)
	{
	case ESOLScavState::Combat:
		TickCombat(DeltaSeconds);
		break;
	case ESOLScavState::Patrol:
	default:
		TickPatrol(DeltaSeconds);
		break;
	}
}

bool ASOLScavenger::CanSee(const ASOLCharacter* Other) const
{
	if (!Other || Other->IsDead())
	{
		return false;
	}

	const FVector MyEye = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
	const FVector TargetEye = Other->GetActorLocation() + FVector(0.f, 0.f, Other->BaseEyeHeight);

	const float Dist = FVector::Dist(MyEye, TargetEye);
	if (Dist > SightRange)
	{
		return false;
	}

	// Vision cone: dot product against the facing direction.
	const FVector ToTarget = (TargetEye - MyEye).GetSafeNormal();
	const float CosLimit = FMath::Cos(FMath::DegreesToRadians(SightHalfAngleDeg));
	if (FVector::DotProduct(GetActorForwardVector(), ToTarget) < CosLimit)
	{
		return false;
	}

	// Line of sight. ECC_Pawn for the same reason the weapon trace uses it:
	// the Visibility channel is ignored by pawn capsules.
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SOLScavSight), false, this);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Other);
	FHitResult Blocker;
	const bool bBlocked = World->LineTraceSingleByChannel(Blocker, MyEye, TargetEye, ECC_Pawn, Params);
	return !bBlocked;
}

void ASOLScavenger::UpdatePerception()
{
	// Only human-controlled pawns are considered hostile. Scavengers ignoring
	// each other is a deliberate simplification: NPC infighting would look
	// like a bug and would empty the map before the player arrived.
	ASOLCharacter* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (TActorIterator<ASOLCharacter> It(GetWorld()); It; ++It)
	{
		ASOLCharacter* Other = *It;
		if (!Other || Other == this || Other->IsDead())
		{
			continue;
		}
		if (!Cast<APlayerController>(Other->GetController()))
		{
			continue; // another scavenger
		}
		// A scavenger is a guard, not a hunter: only pawns near the guarded
		// container are hostile. Without this, a patrolling guard wanders
		// inside sight range of the spawn area (patrol radius + compact map)
		// and farms freshly-spawned players — 6 deaths / 70 shots in one
		// verification run (2026-09-03). With it, the spawn field is safe by
		// construction and attacking a guard is always the player's choice.
		if (FVector::Dist2D(Other->GetActorLocation(), HomeLocation) > GuardRadius)
		{
			continue;
		}
		if (!CanSee(Other))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(GetActorLocation(), Other->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			Best = Other;
			BestDistSq = DistSq;
		}
	}

	if (Best)
	{
		// Do not pick up a new target while already dragged off the post: the
		// leash check in TickCombat would immediately disengage again, and the
		// pair would flip-flop every quarter second.
		const float FromHome = FVector::Dist2D(GetActorLocation(), HomeLocation);
		if (FromHome > LeashRange)
		{
			Best = nullptr;
		}
	}

	if (Best)
	{
		if (Target.Get() != Best)
		{
			UE_LOG(LogSOL, Log, TEXT("Scavenger %s acquired %s at %.0fcm"),
				*GetName(), *Best->GetName(), FMath::Sqrt(BestDistSq));
		}
		Target = Best;
		TimeSinceSeenTarget = 0.f;
		State = ESOLScavState::Combat;
		return;
	}

	// Nothing visible: keep hunting the last known target for a while, then
	// give up. The delay is what makes breaking line of sight a real escape
	// rather than an instant reset.
	if (State == ESOLScavState::Combat)
	{
		TimeSinceSeenTarget += 0.25f;
		if (TimeSinceSeenTarget >= LoseTargetSeconds || !Target.IsValid() || Target->IsDead())
		{
			UE_LOG(LogSOL, Log, TEXT("Scavenger %s lost its target, back to patrol"), *GetName());
			Target = nullptr;
			State = ESOLScavState::Patrol;
			PickNewPatrolPoint();
		}
	}
}

void ASOLScavenger::PickNewPatrolPoint()
{
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float Radius = FMath::FRandRange(PatrolRadius * 0.35f, PatrolRadius);
	PatrolPoint = HomeLocation + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
	PatrolPauseLeft = FMath::FRandRange(PatrolPauseMin, PatrolPauseMax);
}

void ASOLScavenger::FaceLocation(const FVector& TargetLocation, float DeltaSeconds)
{
	// Rotate the pawn directly instead of steering through the controller.
	//
	// AAIController ships with bSetControlRotationFromPawnOrientation enabled,
	// which copies the pawn's rotation *into* the control rotation every frame.
	// Combined with bUseControllerRotationYaw that forms a loop where the
	// controller only ever echoes back where the pawn already points, so
	// SetControlRotation is silently discarded — the scavenger never turned at
	// all. That is what the fourth smoke test was really showing: the one shot
	// that did land came from a spawn rotation that happened to face the
	// player, and the aim cone / firing gate (both derived from the facing)
	// never opened otherwise.
	const FVector Flat(TargetLocation.X, TargetLocation.Y, GetActorLocation().Z);
	const FRotator Desired = (Flat - GetActorLocation()).Rotation();
	const FRotator Smoothed = FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Desired.Yaw, 0.f), DeltaSeconds, 6.f);
	SetActorRotation(FRotator(0.f, Smoothed.Yaw, 0.f));
}

void ASOLScavenger::TickPatrol(float DeltaSeconds)
{
	// Safety net: if the scavenger has somehow drifted well past its patrol
	// radius, walk home instead of picking another random point out there.
	const float FromHome = FVector::Dist2D(GetActorLocation(), HomeLocation);
	if (FromHome > PatrolRadius * 1.6f)
	{
		PatrolPoint = HomeLocation;
	}

	const float DistSq = FVector::DistSquared2D(GetActorLocation(), PatrolPoint);
	if (DistSq < FMath::Square(120.f))
	{
		// Arrived: stand still for a beat, then pick somewhere else. The pause
		// is what makes it read as "a person looking around" instead of a
		// vacuum robot.
		PatrolPauseLeft -= DeltaSeconds;
		if (PatrolPauseLeft <= 0.f)
		{
			PickNewPatrolPoint();
		}
		return;
	}

	FaceLocation(PatrolPoint, DeltaSeconds);

	// Steer by the direction to the target, not by the current facing. Facing
	// is interpolated (FaceLocation smooths the turn), so moving along
	// GetActorForwardVector() sends the pawn off at whatever angle it happens
	// to be mid-turn — it overshoots, turns back, overshoots again, and the
	// path spirals outward. The third smoke test caught exactly this: a
	// scavenger with a 900 cm patrol radius reported being 3200 cm from its
	// post. Movement input and facing are separate concerns.
	const FVector ToPoint = (PatrolPoint - GetActorLocation()).GetSafeNormal2D();
	AddMovementInput(ToPoint, 0.55f); // walk, not jog
}

void ASOLScavenger::TickCombat(float DeltaSeconds)
{
	ASOLCharacter* Foe = Target.Get();
	if (!Foe || Foe->IsDead())
	{
		State = ESOLScavState::Patrol;
		Target = nullptr;
		PickNewPatrolPoint();
		return;
	}

	// Leash: a guard that has been pulled too far from its post gives up and
	// walks home, whatever the target is doing. Checked before anything else
	// so the return trip cannot be interrupted by re-acquiring the same
	// player (UpdatePerception only promotes to Combat, never past the leash).
	const float FromHome = FVector::Dist2D(GetActorLocation(), HomeLocation);
	if (FromHome > LeashRange)
	{
		UE_LOG(LogSOL, Log, TEXT("Scavenger %s hit its leash (%.0fcm from post), disengaging"),
			*GetName(), FromHome);
		Target = nullptr;
		State = ESOLScavState::Patrol;
		// Head straight back rather than to a random patrol point, so the
		// retreat reads as "returning to post".
		PatrolPoint = HomeLocation;
		PatrolPauseLeft = 0.f;
		return;
	}

	FaceLocation(Foe->GetActorLocation(), DeltaSeconds);

	const float Dist = FVector::Dist2D(GetActorLocation(), Foe->GetActorLocation());
	if (Dist > PreferredCombatRange)
	{
		// Same rule as patrol: move along the direction to the target, not the
		// (still turning) facing, or the approach curves away and the leash
		// trips before the scavenger ever gets in range.
		const FVector ToFoe2D = (Foe->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		AddMovementInput(ToFoe2D, 1.f);
	}

	// Only shoot when it is actually looking at the target — FaceLocation
	// interpolates, so firing during the turn would spray the landscape.
	// 0.95 (about 18 degrees) rather than a tighter gate: the aim cone is 5
	// degrees wide anyway, and a stricter threshold means a scavenger that is
	// still settling its turn never gets to fire at all.
	const FVector ToFoe = (Foe->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToFoe) > 0.95f && CanSee(Foe))
	{
		// TryFire handles the rate limit, the empty magazine and the reload.
		// Calling it on the server runs ServerFireShot's implementation
		// directly, so the NPC goes through the exact same authoritative
		// shot path as a player.
		TryFire();
	}
}

void ASOLScavenger::GetAimRay(FVector& OutStart, FVector& OutDir) const
{
	OutStart = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);

	// Aim at the target's chest rather than straight ahead, then apply the
	// spread cone. Without the cone a hitscan NPC never misses.
	FVector Aim = GetActorForwardVector();
	if (const ASOLCharacter* Foe = Target.Get())
	{
		Aim = (Foe->GetActorLocation() + FVector(0.f, 0.f, 40.f) - OutStart).GetSafeNormal();
	}
	OutDir = FMath::VRandCone(Aim, FMath::DegreesToRadians(AimSpreadDeg));
}

void ASOLScavenger::HandleDeath(AActor* Victim, AController* Killer)
{
	// Base class does the important half: spill the backpack, credit the
	// killer, stop the movement, play the death clip.
	Super::HandleDeath(Victim, Killer);

	if (!HasAuthority())
	{
		return;
	}

	State = ESOLScavState::Patrol;
	Target = nullptr;

	// Let the corpse linger briefly so the kill reads, then clean up — the
	// loot is already on the ground and does not depend on the body.
	SetLifeSpan(15.f);

	UE_LOG(LogSOL, Log, TEXT("Scavenger %s down, corpse despawns in 15s"), *GetName());
}
