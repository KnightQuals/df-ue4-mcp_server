// Copyright Epic Games, Inc. All Rights Reserved.

#include "BattleDefenderCamp.h"
#include "UObject/ConstructorHelpers.h"

// Sets default values
ABattleDefenderCamp::ABattleDefenderCamp()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;

	CampMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CampMesh"));
	CampMesh->SetupAttachment(RootComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'"));
	if (DefaultMesh.Succeeded())
	{
		CampMesh->SetStaticMesh(DefaultMesh.Object);
	}
}

// Called when the game starts or when spawned
void ABattleDefenderCamp::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("BattleDefenderCamp spawned"));
}
