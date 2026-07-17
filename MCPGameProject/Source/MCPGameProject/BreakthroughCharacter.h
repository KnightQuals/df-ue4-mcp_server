// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BreakthroughCharacter.generated.h"

// Playable character for the Breakthrough game mode. Team assignment (0 = attacker,
// 1 = defender) drives which side of the capture logic this pawn counts toward.
// V1 simplification: no replication, default character movement component.
UCLASS()
class MCPGAMEPROJECT_API ABreakthroughCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABreakthroughCharacter();

	// Team this character belongs to: 0 = attackers, 1 = defenders.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	int32 Team = 0;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
};
