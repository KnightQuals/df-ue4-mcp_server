// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpawnAreaHub.h"

// Sets default values
ASpawnAreaHub::ASpawnAreaHub()
{
	// This actor is a pure data/logic container; it never needs to tick.
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;
}

// Called when the game starts or when spawned
void ASpawnAreaHub::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("SpawnAreaHub spawned, points=%d"), SpawnPoints.Num());
}

AActor* ASpawnAreaHub::GetRandomSpawnPoint() const
{
	if (SpawnPoints.Num() == 0)
	{
		return nullptr;
	}

	const int32 Index = FMath::RandRange(0, SpawnPoints.Num() - 1);
	return SpawnPoints[Index];
}
