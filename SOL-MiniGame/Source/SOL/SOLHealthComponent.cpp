// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLHealthComponent.h"
#include "SOL.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"

USOLHealthComponent::USOLHealthComponent()
{
	// Regeneration needs a tick, but 10Hz is plenty for a 4hp/s trickle —
	// there is no reason to spend a full-frame tick on arithmetic this small.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;

	// SetIsReplicatedByDefault is protected on UActorComponent, so it has to
	// be called from the component's own constructor — not from the owning
	// actor's (that was a C2248 the first time round).
	SetIsReplicatedByDefault(true);
}

void USOLHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	LastKnownHealth = MaxHealth;

	// Damage is adjudicated on the server only. Binding the delegate on
	// clients too would double-apply the local prediction and desync the bar.
	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		Owner->SetCanBeDamaged(true);
		Owner->OnTakeAnyDamage.AddDynamic(this, &USOLHealthComponent::HandleTakeAnyDamage);
		BeginSpawnProtection();
	}
}

void USOLHealthComponent::BeginSpawnProtection()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	ProtectionLeft = SpawnProtectSeconds;
}

void USOLHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceLastDamage += DeltaTime;

	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bDead)
	{
		return;
	}

	if (ProtectionLeft > 0.f)
	{
		ProtectionLeft = FMath::Max(0.f, ProtectionLeft - DeltaTime);
	}

	if (Health >= MaxHealth || TimeSinceLastDamage < RegenDelay || RegenPerSecond <= 0.f)
	{
		return;
	}
	Health = FMath::Min(MaxHealth, Health + RegenPerSecond * DeltaTime);
}

void USOLHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	AController* InstigatedBy, AActor* DamageCauser)
{
	if (bDead || Damage <= 0.f)
	{
		return;
	}

	// Spawn protection swallows the hit entirely. Logged at Log rather than
	// Verbose so a verification pass can actually see it: "why did my shot do
	// nothing" needs an answer in the default log, and the window is only a
	// few seconds so it cannot spam.
	if (ProtectionLeft > 0.f)
	{
		UE_LOG(LogSOL, Log, TEXT("Damage ignored: %s is spawn-protected for %.1fs more"),
			*GetNameSafe(DamagedActor), ProtectionLeft);
		return;
	}

	Health = FMath::Max(0.f, Health - Damage);
	TimeSinceLastDamage = 0.f;

	UE_LOG(LogSOL, Log, TEXT("Damage: %s took %.0f from %s -> %.0f/%.0f hp"),
		*GetNameSafe(DamagedActor), Damage,
		InstigatedBy ? *GetNameSafe(InstigatedBy) : TEXT("<world>"),
		Health, MaxHealth);

	if (Health <= 0.f)
	{
		// Latch before broadcasting: the death handler destroys or respawns
		// the owner, and any bullet already in flight this frame must not
		// settle the same corpse a second time.
		bDead = true;
		UE_LOG(LogSOL, Log, TEXT("DEATH: %s killed by %s"),
			*GetNameSafe(DamagedActor), InstigatedBy ? *GetNameSafe(InstigatedBy) : TEXT("<world>"));
		OnDeath.ExecuteIfBound(DamagedActor, InstigatedBy);
	}
}

void USOLHealthComponent::ResetHealth()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	Health = MaxHealth;
	bDead = false;
	TimeSinceLastDamage = 999.f;
	BeginSpawnProtection();
}

void USOLHealthComponent::OnRep_Health()
{
	// Presentation only. A drop means "I got hit" — the HUD flashes the
	// damage vignette off this, so hit feedback costs zero extra networking
	// (the health value was going to replicate anyway).
	if (Health < LastKnownHealth)
	{
		const float Delta = LastKnownHealth - Health;
		UE_LOG(LogSOL, Verbose, TEXT("OnRep_Health: -%.0f (now %.0f)"), Delta, Health);
	}
	LastKnownHealth = Health;
}

void USOLHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USOLHealthComponent, Health);
	DOREPLIFETIME(USOLHealthComponent, bDead);
	DOREPLIFETIME(USOLHealthComponent, ProtectionLeft);
}
