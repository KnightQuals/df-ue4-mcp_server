// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SOLItemTypes.generated.h"

// One lootable item definition (what can come out of a container).
// DisplayName is Chinese for the HUD (Canvas text has CJK font fallback);
// ItemID stays ASCII because it is also used for world TextRender labels,
// which use the legacy render path with no CJK glyphs.
USTRUCT(BlueprintType)
struct FSOLItemDef
{
	GENERATED_BODY()

	// Stable ASCII id, e.g. "HEART_OF_AFRICA". Used as DataTable key and world label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Item")
	FName ItemID = NAME_None;

	// Chinese display name shown on the HUD panels.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Item")
	FString DisplayName;

	// How heavy one unit is (kg). Backpack capacity is measured in kg.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Item")
	float WeightKg = 1.f;

	// Spawn probability weight inside a container's loot pool.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Item")
	float SpawnWeight = 1.f;

	// Extraction payout per unit. The whole point of a search-and-extract
	// round is that loot only counts once you carry it out alive, so every
	// item needs a number the settlement screen can add up. Kept on the
	// item definition (not a separate table) so a DataTable row configures
	// pool + weight + value in one place.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Item")
	int32 Value = 100;
};

// A concrete stack of one item type (definition + count).
USTRUCT(BlueprintType)
struct FSOLItemInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Item")
	FSOLItemDef Def;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Item")
	int32 Count = 0;

	float TotalWeightKg() const { return Def.WeightKg * Count; }

	int32 TotalValue() const { return Def.Value * Count; }
};

// One row of DT_SOLContainers: the loot configuration for a single container
// actor. Row key = the container's actor name (or ConfigRow override), so
// MCP-spawned containers pick up their pool purely by naming convention —
// no per-actor property writes needed (array properties don't go over
// reflection anyway). Design doc requirement: 多容器各配置道具与权重.
USTRUCT(BlueprintType)
struct FSOLContainerRow : public FTableRowBase
{
	GENERATED_BODY()

	// ASCII label (legacy world TextRender labels; TextRender only accepts
	// offline fonts which have no CJK glyphs, so the 3D label stays ASCII).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Container")
	FName ContainerName = TEXT("BOX");

	// Chinese display name used by the HUD panel title and the screen-space
	// container nameplates. Empty falls back to a built-in ASCII->Chinese
	// mapping in ASOLContainer::GetDisplayNameZh().
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Container")
	FString DisplayNameZh;

	// How many draws the first-open roll performs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Container")
	int32 NumRolls = 3;

	// The weighted loot pool for this container.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Container")
	TArray<FSOLItemDef> Items;
};

// One row of DT_SOLExtractions: tuning for a single extraction zone actor.
// Same convention as containers — row key = the zone's actor name (or its
// ConfigRow override) — so an MCP-spawned zone picks up its configuration by
// naming alone, with no per-actor property writes.
USTRUCT(BlueprintType)
struct FSOLExtractionRow : public FTableRowBase
{
	GENERATED_BODY()

	// Chinese display name for the HUD nameplate / settlement screen.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Extraction")
	FString DisplayNameZh;

	// Horizontal radius of the zone in cm.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Extraction")
	float Radius = 400.f;

	// How long the player must stay inside, in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Extraction")
	float HoldSeconds = 10.f;

	// Payout multiplier applied to the carried loot value. A riskier zone
	// (far from spawn, out in the open) can pay better than a safe one.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Extraction")
	float ValueMultiplier = 1.f;

	// Minimum carried value required to be allowed to extract. 0 = always
	// open; a positive number turns the zone into a "bring something back"
	// objective instead of a free exit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SOL|Extraction")
	int32 MinValueRequired = 0;
};
