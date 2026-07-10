// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "BattleCampSector.generated.h"

UCLASS()
class MCPGAMEPROJECT_API ABattleCampSector : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABattleCampSector();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Visible mesh for the camp sector base shape (cube).
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* CampMesh;

	// Team id: 0 = attackers (this camp is the attacker camp).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	int32 TeamId = 0;
};
