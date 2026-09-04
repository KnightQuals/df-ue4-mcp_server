// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SOLItemTypes.h"
#include "SOLItemPickup.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UTextRenderComponent;

// A dropped/pickupable item stack lying in the world. Spawned when a player
// drops something from the backpack; picking it up is the same F-interaction
// as containers (nearest-wins scan in the player controller).
UCLASS()
class SOL_API ASOLItemPickup : public AActor
{
	GENERATED_BODY()

public:
	ASOLItemPickup();

	UPROPERTY(VisibleAnywhere, Category = "SOL|Pickup")
	USphereComponent* PickupSphere = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "SOL|Pickup")
	UStaticMeshComponent* ItemMesh = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "SOL|Pickup")
	UTextRenderComponent* LabelComponent = nullptr;

	// The carried stack. Replicated: the server spawns drop pickups and the
	// data ships to every client; OnRep refreshes the world label there
	// (the server itself has no rendering, it only sets the data).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Data, Category = "SOL|Pickup")
	FSOLItemInstance Data;

	// Refreshes the world label from the carried data (ASCII id + count).
	void RefreshLabel();

	// Spawns a pickup snapped to the ground. A naive SpawnActor at the
	// carrier's location leaves the drop hovering in the air: the corpse
	// capsule still has collision, so AdjustIfPossibleButAlwaysSpawn pushes
	// the new actor up on top of it (user-reported 2026-09-03: drops floating
	// above a killed scavenger). This traces straight down from above the
	// requested spot and places the pickup on whatever it hits (floor, rock,
	// container top) — and uses AlwaysSpawn so nothing else can shove it.
	static ASOLItemPickup* SpawnGrounded(UWorld* World, const FVector& ApproxLocation, const FSOLItemInstance& InData);

protected:
	UFUNCTION()
	void OnRep_Data();

	// See ASOLContainer::PostLoad — keeps the networked defaults on any
	// instance that comes from a saved map instead of a fresh spawn.
	virtual void PostLoad() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
