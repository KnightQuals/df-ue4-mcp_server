// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForbiddenZone.generated.h"

class UBoxComponent;
class UTextRenderComponent;
class UStaticMeshComponent;

// A V2 outer-map danger strip. Four of these form the rectangular forbidden zone
// around the battlefield. Server-side overlap notifications drive each character's
// replicated 10-second elimination countdown.
UCLASS()
class MCPGAMEPROJECT_API AForbiddenZone : public AActor
{
	GENERATED_BODY()

public:
	AForbiddenZone();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forbidden Zone")
	UBoxComponent* TriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forbidden Zone")
	UTextRenderComponent* WarningLabel;

	// A thin red ground bar that makes the V2 outer boundary readable in the level.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forbidden Zone")
	UStaticMeshComponent* BorderVisual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forbidden Zone")
	FString ZoneLabel = TEXT("FORBIDDEN ZONE");

	// Configures label text/extent after GameMode spawns this border segment.
	void ConfigureZone(const FString& InZoneLabel, const FVector& InExtent);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
