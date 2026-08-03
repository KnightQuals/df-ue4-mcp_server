// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "BattleSectorAnchor.generated.h"

class UTextRenderComponent;

UCLASS()
class MCPGAMEPROJECT_API ABattleSectorAnchor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABattleSectorAnchor();

	// Owning team: 0 = attackers, 1 = defenders. Default: defenders own the sector.
	// Replicated with OnRep so clients recolor the anchor when ownership changes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_OwningTeam)
	int32 OwningTeam = 1;

	// Radius (cm) of the capture influence sphere around this anchor.
	UPROPERTY(EditAnywhere)
	float CaptureRadius = 800.0f;

	// Capture progress in [-1, 1]: -1 = defenders, 0 = neutral, 1 = attackers. Default: defenders.
	// Replicated so clients can show a progress bar / debug value if desired.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
	float CaptureProgress = -1.f;

	// Capture progress per second of advantage (|attackers - defenders| in zone).
	UPROPERTY(EditAnywhere)
	float CaptureSpeed = 1.f;

	// Box trigger used to detect actors entering/leaving the capture zone. Smaller
	// than the 8m sphere (so the SpawnHubs 4m away don't overlap the trigger) and
	// axis-aligned so the trigger area is predictable.
	UPROPERTY(VisibleAnywhere)
	UBoxComponent* CaptureZone;

	// Floating 3D label above the anchor, e.g. "CAPTURE ZONE" (recolored on capture).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UTextRenderComponent* AreaLabel;

	// NOTE: The visible "AnchorMesh" StaticMeshComponent is added in the Blueprint, not in C++.
	// It is located at runtime via FindComponentByClass<UStaticMeshComponent>() in BeginPlay.

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Replication.
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	// Called on clients when OwningTeam replicates — recolors the anchor + label.
	UFUNCTION()
	void OnRep_OwningTeam();

	UFUNCTION()
	void OnCaptureZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnCaptureZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// Dynamic material instance used to recolor AnchorMesh at runtime.
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* AnchorMID = nullptr;

	// Recolor AnchorMesh to represent the current owning team.
	void ApplyAnchorColor();

	// Resolve which team an overlapping actor belongs to.
	// Returns: 0 = attacker, 1 = defender, -1 = unknown (not counted).
	// Override in blueprint or subclass to plug in your own team system.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Battle")
	int32 GetActorTeam(AActor* OtherActor) const;
	virtual int32 GetActorTeam_Implementation(AActor* OtherActor) const;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Count of attackers currently inside the capture zone.
	UPROPERTY(Transient)
	int32 AttackersInZone = 0;

	// Count of defenders currently inside the capture zone.
	UPROPERTY(Transient)
	int32 DefendersInZone = 0;
};
