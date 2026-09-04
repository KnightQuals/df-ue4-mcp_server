// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SOLItemTypes.h"
#include "SOLExtractionZone.generated.h"

class UStaticMeshComponent;
class UCylinderComponent;
class UCapsuleComponent;
class ASOLCharacter;

// Per-player extraction progress, replicated as an array so every client can
// draw the progress of anyone standing in the zone (a teammate extracting is
// information you want to see). Kept as a small struct rather than a map
// because UE replication does not support TMap.
USTRUCT()
struct FSOLExtractionProgress
{
	GENERATED_BODY()

	// The extracting player's controller. Weak semantics are not needed here:
	// the server clears entries the moment a pawn leaves or is destroyed.
	UPROPERTY()
	APlayerState* Player = nullptr;

	// Seconds held so far (server authoritative, replicated for the HUD ring).
	UPROPERTY()
	float Elapsed = 0.f;
};

// An extraction point: the round's win condition. Loot only counts once it is
// carried out, so the player must stand inside the zone for HoldSeconds
// without leaving. The server owns the timer and the settlement; clients only
// draw the ring and the result screen.
//
// Design notes:
//  - Server authoritative by construction: the timer only advances inside
//    Tick on HasAuthority(), so a tampered client cannot self-extract.
//  - Leaving resets progress to zero (no partial credit), which is what
//    makes an extraction a commitment rather than a formality.
//  - Configuration comes from DT_SOLExtractions keyed by actor name, the
//    same convention the containers use.
UCLASS()
class SOL_API ASOLExtractionZone : public AActor
{
	GENERATED_BODY()

public:
	ASOLExtractionZone();

	// Evaluation runs on a repeating timer instead of Tick. Two reasons:
	//  1) Recompiling a blueprint whose CDO has bCanEverTick=true while
	//     instances live in the editor world triggers a pure-virtual crash in
	//     UE4.27 (the old tick function is destroyed but stays registered) —
	//     hit for real during MCP provisioning;
	//  2) extraction is a 8-15s hold, so 10Hz is plenty and costs ~1/6 of a
	//     per-frame check.
	void EvaluateZone();

	// Visual footprint (a flat translucent disc) and the interaction volume.
	UPROPERTY(VisibleAnywhere, Category = "SOL|Extraction")
	UStaticMeshComponent* ZoneMesh = nullptr;

	// Marker pillar so the zone is visible from across the map.
	UPROPERTY(VisibleAnywhere, Category = "SOL|Extraction")
	UStaticMeshComponent* BeaconMesh = nullptr;

	UPROPERTY(EditAnywhere, Category = "SOL|Extraction")
	FString DisplayNameZh = TEXT("撤离点");

	// Horizontal radius in cm. Vertical extent is deliberately generous
	// (the zone is a column, not a sphere) so standing on a rock inside the
	// disc still counts.
	UPROPERTY(EditAnywhere, Replicated, Category = "SOL|Extraction")
	float Radius = 400.f;

	UPROPERTY(EditAnywhere, Replicated, Category = "SOL|Extraction")
	float HoldSeconds = 10.f;

	UPROPERTY(EditAnywhere, Replicated, Category = "SOL|Extraction")
	float ValueMultiplier = 1.f;

	UPROPERTY(EditAnywhere, Replicated, Category = "SOL|Extraction")
	int32 MinValueRequired = 0;

	// Config table lookup (row key = actor name unless ConfigRow is set).
	UPROPERTY(EditAnywhere, Category = "SOL|Extraction|Config")
	FString ConfigTablePath = TEXT("/Game/Config/DT_SOLExtractions.DT_SOLExtractions");

	UPROPERTY(EditAnywhere, Category = "SOL|Extraction|Config")
	FName ConfigRow = NAME_None;

	// Live progress of everyone currently inside (server writes, all clients read).
	UPROPERTY(Replicated)
	TArray<FSOLExtractionProgress> Progress;

	// True horizontal containment test (column, not sphere).
	bool IsPawnInside(const APawn* Pawn) const;

	// Progress for one player in [0,1]; 0 when not extracting.
	float GetProgressFor(const APlayerState* Player) const;

	FString GetDisplayName() const { return DisplayNameZh.IsEmpty() ? TEXT("撤离点") : DisplayNameZh; }

protected:
	virtual void BeginPlay() override;
	virtual void PostLoad() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	bool TryLoadConfigRow();

	// Applies the zone's radius to the visual disc so designers/MCP only
	// have to set Radius (or the DataTable row) and the art follows.
	void SyncVisualScale();

	// Completes extraction for one player: settle the backpack into a score
	// and mark the player as extracted.
	void CompleteExtraction(class ASOLPlayerController* PC);

	// Server-side evaluation timer (10Hz).
	FTimerHandle EvalTimerHandle;
	static constexpr float EvalInterval = 0.1f;

	// Players already told "you need more loot to use this exit", so the
	// message fires once per approach instead of ten times a second. Server
	// only — never replicated (it is bookkeeping, not game state).
	TSet<APlayerState*> IneligibleReported;
};
