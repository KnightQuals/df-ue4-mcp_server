// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SOLItemTypes.h"
#include "SOLContainer.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UTextRenderComponent;

// A lootable container (birdcage / box / luggage ...). The first time it is
// opened, the contents are rolled once from the loot pool (weighted pick,
// NumRolls draws) and then stay fixed for the rest of the match — per the
// design doc, "第一次打开容器决定道具".
// If ItemPool is empty at BeginPlay, a deterministic default pool is chosen
// from the actor name hash so MCP-spawned containers get variety without
// needing array properties to be set over reflection (which the current
// set_actor_property does not support).
UCLASS()
class SOL_API ASOLContainer : public AActor
{
	GENERATED_BODY()

public:
	ASOLContainer();

	// Interaction volume. The player controller scans for the nearest
	// container whose InteractSphere contains the pawn.
	UPROPERTY(VisibleAnywhere, Category = "SOL|Container")
	USphereComponent* InteractSphere = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "SOL|Container")
	UStaticMeshComponent* ContainerMesh = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "SOL|Container")
	UTextRenderComponent* LabelComponent = nullptr;

	// ASCII name; also selects the default loot pool when ItemPool is empty.
	UPROPERTY(EditAnywhere, Category = "SOL|Container")
	FName ContainerName = TEXT("BOX");

	// Chinese display name (HUD panel title / screen-space nameplate).
	// Empty -> built-in fallback mapping (CAGE->鸟笼 etc).
	UPROPERTY(EditAnywhere, Category = "SOL|Container")
	FString DisplayNameZh;

	// Loot pool. If left empty, a default pool is derived from the actor name.
	UPROPERTY(EditAnywhere, Category = "SOL|Container")
	TArray<FSOLItemDef> ItemPool;

	// How many draws the first-open roll performs.
	UPROPERTY(EditAnywhere, Category = "SOL|Container", meta = (ClampMin = "1", ClampMax = "10"))
	int32 NumRolls = 3;

	// Loot config table. Loaded lazily at BeginPlay; row key = ConfigRow when
	// set, otherwise the actor name (e.g. "SOL_Cage_1"). Missing table or row
	// falls back to the deterministic name-hash default pools.
	UPROPERTY(EditAnywhere, Category = "SOL|Container|Config")
	FString ConfigTablePath = TEXT("/Game/Config/DT_SOLContainers.DT_SOLContainers");

	// Optional explicit row key override (defaults to the actor name).
	UPROPERTY(EditAnywhere, Category = "SOL|Container|Config")
	FName ConfigRow = NAME_None;

	// Rolled contents; generated once on first open. Replicated: the server
	// rolls the loot (see Open) and every client sees what is left in the
	// container, so two players looting the same container stay consistent.
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_Contents, Category = "SOL|Container")
	TArray<FSOLItemInstance> Contents;

	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_bOpened, Category = "SOL|Container")
	bool bOpened = false;

	// First open rolls the loot. Subsequent opens are free peeks.
	// Server authoritative: only rolls on the authority (dedicated server /
	// listen host); harmless no-op when called on a client replica.
	void Open();

	// Removes and returns the stack at the given index (double-click transfer).
	// Server authoritative: client replicas return an empty stack.
	FSOLItemInstance TakeItemAt(int32 Index);

	// Server-side: re-lock for a new round. Clearing Contents and bOpened is
	// enough — the next Open() re-rolls from the pool, so a fresh round gets
	// fresh loot rather than whatever was left behind.
	void ResetForNewRound();

	// Chinese name for HUD display: DisplayNameZh if set, else a built-in
	// ASCII->Chinese mapping, else the raw ASCII name.
	FString GetDisplayNameZh() const;

	bool HasItems() const { return Contents.Num() > 0; }

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Contents();

	UFUNCTION()
	void OnRep_bOpened();

private:
	// Try to load this container's loot config row from the DataTable.
	// Returns true when a row was found and applied.
	bool TryLoadConfigRow();

	// Deterministic default pools keyed by actor name hash, so three
	// MCP-spawned containers named differently get different loot tables.
	void ApplyDefaultPool();
};
