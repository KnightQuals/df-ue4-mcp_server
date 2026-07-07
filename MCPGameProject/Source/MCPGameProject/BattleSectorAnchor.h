// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "BattleSectorAnchor.generated.h"

UCLASS()
class MCPGAMEPROJECT_API ABattleSectorAnchor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABattleSectorAnchor();

	// 0 = neutral, 1 = attackers, 2 = defenders.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 OwningTeam = 0;

	// Radius (cm) of the capture influence sphere around this anchor.
	UPROPERTY(EditAnywhere)
	float CaptureRadius = 300.0f;

	// Capture progress 0~100.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CaptureProgress = 0.f;

	// Capture progress per second while attackers are inside the zone.
	UPROPERTY(EditAnywhere)
	float CaptureSpeed = 20.f;

	// Sphere used to detect actors entering/leaving the capture zone.
	UPROPERTY(VisibleAnywhere)
	USphereComponent* CaptureZone;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnCaptureZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnCaptureZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Count of attackers currently inside the capture zone.
	int32 AttackersInZone = 0;
};
