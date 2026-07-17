// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpawnAreaHub.generated.h"

// Groups a set of spawn point actors (e.g. Target Points placed in the level)
// and hands out a random one on request. V1 simplification: no replication —
// spawn selection is a local/server-only concern for now (see V2).
UCLASS()
class MCPGAMEPROJECT_API ASpawnAreaHub : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ASpawnAreaHub();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Spawn point actors (e.g. Target Points) that this hub can hand out.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Spawning")
	TArray<AActor*> SpawnPoints;

	// Returns a random spawn point from SpawnPoints, or nullptr if the array is empty.
	UFUNCTION(BlueprintCallable, Category = "Battle|Spawning")
	AActor* GetRandomSpawnPoint() const;
};
