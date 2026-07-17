// Copyright Epic Games, Inc. All Rights Reserved.

#include "BreakthroughCharacter.h"

// Sets default values
ABreakthroughCharacter::ABreakthroughCharacter()
{
	// Set this character to call Tick() every frame. You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ABreakthroughCharacter::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("Character spawned team=%d"), Team);
}
