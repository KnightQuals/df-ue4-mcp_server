// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLBackpackComponent.h"
#include "SOL.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

USOLBackpackComponent::USOLBackpackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Replicated component (owner-only contents). SetIsReplicatedByDefault is
	// protected on UActorComponent on purpose — replication must be opted into
	// by the component itself, not toggled from the owning actor.
	SetIsReplicatedByDefault(true);
}

float USOLBackpackComponent::CurrentWeightKg() const
{
	float Sum = 0.f;
	for (const FSOLItemInstance& Inst : Items)
	{
		Sum += Inst.TotalWeightKg();
	}
	return Sum;
}

void USOLBackpackComponent::BeginPlay()
{
	Super::BeginPlay();

	// Capture the character's designed speed as the unencumbered baseline
	// before anything scales it. Runs on server and clients alike — both need
	// the same baseline to derive the same penalty.
	if (const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
		{
			BaseWalkSpeed = Movement->MaxWalkSpeed;
		}
	}

	// An empty backpack means no penalty, but apply once anyway so a pawn that
	// spawns with contents (future feature: starting gear) is correct from
	// frame one.
	ApplyEncumbranceToOwner();
}

bool USOLBackpackComponent::CanFit(const FSOLItemInstance& In) const
{
	return CurrentWeightKg() + In.TotalWeightKg() <= MaxWeightKg + KINDA_SMALL_NUMBER;
}

int32 USOLBackpackComponent::CurrentValue() const
{
	int32 Sum = 0;
	for (const FSOLItemInstance& Inst : Items)
	{
		Sum += Inst.TotalValue();
	}
	return Sum;
}

float USOLBackpackComponent::GetLoadRatio() const
{
	if (MaxWeightKg <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Clamp(CurrentWeightKg() / MaxWeightKg, 0.f, 1.f);
}

float USOLBackpackComponent::GetSpeedMultiplier() const
{
	const float Ratio = GetLoadRatio();
	if (Ratio <= FreeWeightRatio)
	{
		return 1.f; // light run: no penalty at all
	}
	// Linear falloff across the remaining band. Guard the denominator so a
	// FreeWeightRatio of 1.0 (penalty disabled by config) cannot divide by zero.
	const float Band = FMath::Max(1.f - FreeWeightRatio, KINDA_SMALL_NUMBER);
	const float Penalised = (Ratio - FreeWeightRatio) / Band;
	return FMath::Lerp(1.f, MinSpeedRatio, FMath::Clamp(Penalised, 0.f, 1.f));
}

FString USOLBackpackComponent::GetEncumbranceLabel() const
{
	const float Ratio = GetLoadRatio();
	if (Ratio <= FreeWeightRatio)
	{
		return TEXT("轻装");
	}
	if (Ratio >= 0.95f)
	{
		return TEXT("满载");
	}
	return TEXT("负重");
}

void USOLBackpackComponent::ApplyEncumbranceToOwner()
{
	// The movement component is the single place the penalty lands. Both the
	// server and the owning client run this with identical Items, so the
	// predicted and authoritative speeds match and no correction is triggered.
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	const float Multiplier = GetSpeedMultiplier();
	const float NewSpeed = BaseWalkSpeed * Multiplier;
	if (FMath::IsNearlyEqual(Movement->MaxWalkSpeed, NewSpeed, 0.5f))
	{
		return; // nothing changed; skip the log spam
	}
	Movement->MaxWalkSpeed = NewSpeed;

	UE_LOG(LogSOL, Log, TEXT("Encumbrance %s: %.1f/%.1fkg (%.0f%%) -> speed %.0f (x%.2f) [%s]"),
		OwnerCharacter->HasAuthority() ? TEXT("server") : TEXT("client"),
		CurrentWeightKg(), MaxWeightKg, GetLoadRatio() * 100.f,
		NewSpeed, Multiplier, *GetEncumbranceLabel());
}

void USOLBackpackComponent::OnRep_Items()
{
	// Contents just arrived on the owning client — resync the local movement
	// speed so client-side prediction matches what the server already applied.
	ApplyEncumbranceToOwner();
}

bool USOLBackpackComponent::AddItem(const FSOLItemInstance& In)
{
	// Server authoritative guard: a client-side AddItem would only mutate
	// the local replica and get bounced by the next replication update.
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (In.Count <= 0 || !CanFit(In))
	{
		return false;
	}

	for (FSOLItemInstance& Inst : Items)
	{
		if (Inst.Def.ItemID == In.Def.ItemID)
		{
			Inst.Count += In.Count;
			// Weight changed on the authority — reapply immediately instead of
			// waiting for the replication round trip, so the server's own
			// simulation slows down on the same frame the loot lands.
			ApplyEncumbranceToOwner();
			return true;
		}
	}
	Items.Add(In);
	ApplyEncumbranceToOwner();
	return true;
}

FSOLItemInstance USOLBackpackComponent::RemoveItemAt(int32 Index)
{
	// Server authoritative guard (same reasoning as AddItem).
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Items.IsValidIndex(Index))
	{
		return FSOLItemInstance();
	}
	FSOLItemInstance Out = Items[Index];
	Items.RemoveAt(Index);
	// Dropping loot must restore speed on the same frame, otherwise ditching
	// cargo to outrun someone would feel unresponsive.
	ApplyEncumbranceToOwner();
	return Out;
}

void USOLBackpackComponent::ClearAll()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (Items.Num() == 0)
	{
		return;
	}
	Items.Reset();
	// An emptied pack is a light pack: without this the corpse (or the freshly
	// settled extractor) would keep the 满载 walk speed it died with.
	ApplyEncumbranceToOwner();
}

void USOLBackpackComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// OwnerOnly: your backpack is your business. Weight/contents changes on
	// the server ship to the owning client only — the minimal replication
	// set in action (nobody else's UI ever reads this array).
	DOREPLIFETIME_CONDITION(USOLBackpackComponent, Items, COND_OwnerOnly);
}
