// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SOLItemTypes.h"
#include "SOLBackpackComponent.generated.h"

// Weight-limited backpack. Stores item stacks, enforces a total kg budget.
UCLASS(ClassGroup = (SOL), meta = (BlueprintSpawnableComponent))
class SOL_API USOLBackpackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USOLBackpackComponent();

	UPROPERTY(EditAnywhere, Category = "SOL|Backpack")
	float MaxWeightKg = 30.f;

	// Encumbrance tuning. Carrying loot has to cost something, otherwise the
	// optimal play is always "fill up to 30kg before extracting" and the whole
	// value-density design (1200 per 0.5kg gem vs 60 per 3kg ammo) is
	// meaningless. Below FreeWeightRatio there is no penalty at all, so a light
	// high-value run stays fast; past it the speed falls off linearly to
	// MinSpeedRatio at full capacity.
	UPROPERTY(EditAnywhere, Category = "SOL|Backpack|Encumbrance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FreeWeightRatio = 0.3f;

	UPROPERTY(EditAnywhere, Category = "SOL|Backpack|Encumbrance", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float MinSpeedRatio = 0.55f;

	// Unencumbered speed, captured once at BeginPlay from the character's own
	// MaxWalkSpeed. It must be stored: reading MaxWalkSpeed as the baseline
	// would read back a value this component already scaled down, so every
	// pickup would compound the penalty and the character would creep to a
	// halt after a few transfers.
	UPROPERTY(VisibleAnywhere, Category = "SOL|Backpack|Encumbrance")
	float BaseWalkSpeed = 600.f;

	// Weight fraction of capacity, clamped to [0,1].
	float GetLoadRatio() const;

	// Speed multiplier for the current load, in [MinSpeedRatio, 1].
	// Both the server and the owning client call this and must agree: the
	// backpack replicates COND_OwnerOnly, so both sides hold the same Items
	// array and derive the same number. A mismatch would desync the movement
	// prediction and produce rubber-banding.
	float GetSpeedMultiplier() const;

	// Chinese label for the HUD ("轻装 / 负重 / 满载"), derived from the same
	// ratio so the readout can never disagree with the actual penalty.
	FString GetEncumbranceLabel() const;

	// Weight-limited inventory. Replicated COND_OwnerOnly: the server is the
	// only writer (item transfers arrive as Server RPCs); only the owning
	// player's client receives the contents — nobody else's business.
	// ReplicatedUsing: the owning client has to recompute its movement speed
	// the moment the contents land, or its locally predicted speed would keep
	// disagreeing with the server's.
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_Items, Category = "SOL|Backpack")
	TArray<FSOLItemInstance> Items;

	float CurrentWeightKg() const;

	// Total extraction payout of everything currently carried. Used by the
	// extraction zone settlement and by the HUD's carried-value readout.
	int32 CurrentValue() const;

	bool CanFit(const FSOLItemInstance& In) const;

	// Adds a stack (merging same ids). Returns false when over capacity.
	// Server authoritative: only meaningful when called on the authority.
	bool AddItem(const FSOLItemInstance& In);

	// Removes and returns the stack at the given index.
	// Server authoritative: client replicas return an empty stack.
	FSOLItemInstance RemoveItemAt(int32 Index);

	// Empties the pack in one go (death spill, extraction settlement).
	// Exists as a method rather than letting callers touch Items directly so
	// the "Items changed -> recompute walk speed" invariant holds for every
	// mutation path; a raw Items.Reset() silently skipped it before.
	void ClearAll();

	// Pushes the encumbrance-derived speed onto the owner's movement component.
	// Called on both the server (right after a mutation) and the owning client
	// (from OnRep_Items) so the two never drift apart.
	void ApplyEncumbranceToOwner();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Items();
};
