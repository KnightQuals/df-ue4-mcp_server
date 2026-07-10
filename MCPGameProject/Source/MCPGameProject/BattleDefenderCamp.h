// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "BattleDefenderCamp.generated.h"

UCLASS()
class MCPGAMEPROJECT_API ABattleDefenderCamp : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABattleDefenderCamp();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Visible mesh for the defender camp base shape (cube).
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* CampMesh;

	// Team id: 1 = defenders (this camp is the defender camp).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	int32 TeamId = 1;
};
