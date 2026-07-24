// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpawnAreaHub.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/BoxComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"

// Sets default values
ASpawnAreaHub::ASpawnAreaHub()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;

	// Wireframe box outlining the base area. Drawn in green (attacker) / red (defender)
	// via ShapeColor in BeginPlay. No collision — pure visual.
	AreaBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AreaBox"));
	AreaBox->SetupAttachment(RootComponent);
	AreaBox->SetBoxExtent(FVector(300.f, 300.f, 200.f)); // 6m × 6m × 4m box
	AreaBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AreaBox->SetHiddenInGame(false);
	AreaBox->ShapeColor = FColor(120, 220, 120); // green default; recolored in BeginPlay

	// Floating 3D label above the hub so the player can tell which base this is.
	// Text content + color are set in BeginPlay based on Team.
	AreaLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("AreaLabel"));
	AreaLabel->SetupAttachment(RootComponent);
	AreaLabel->SetRelativeLocation(FVector(0.f, 0.f, 300.f)); // hover 3m above the floor
	AreaLabel->SetTextRenderColor(FColor::White);
	AreaLabel->SetText(FText::FromString(TEXT("BASE")));
	AreaLabel->SetWorldSize(100.f); // text size in world units (big enough to read from across the map)
}

// Called when the game starts or when spawned
void ASpawnAreaHub::BeginPlay()
{
	Super::BeginPlay();

	// Set label text + color by team so the player can read which base they're near.
	if (AreaLabel)
	{
		// TextRenderComponent default faces world +X; rotate so its +X axis points TOWARD
		// the level center, so the player sees the text face-on (not mirror-flipped).
		FVector ToCenter = FVector(0.f, 0.f, 0.f) - GetActorLocation();
		ToCenter.Z = 0.f;
		if (!ToCenter.IsNearlyZero())
		{
			FRotator LookAtCenter = ToCenter.Rotation();
			// Rotate the LABEL (not the whole actor) so the wireframe box stays axis-aligned.
			AreaLabel->SetWorldRotation(FRotator(0.f, LookAtCenter.Yaw + 180.f, 0.f));
		}

		if (Team == 0)
		{
			AreaLabel->SetText(FText::FromString(TEXT("ATTACKER BASE")));
			AreaLabel->SetTextRenderColor(FColor(120, 220, 120)); // green
		}
		else if (Team == 1)
		{
			AreaLabel->SetText(FText::FromString(TEXT("DEFENDER BASE")));
			AreaLabel->SetTextRenderColor(FColor(220, 120, 120)); // red
		}
		else
		{
			AreaLabel->SetText(FText::FromString(TEXT("BASE")));
			AreaLabel->SetTextRenderColor(FColor::White);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("SpawnAreaHub spawned, team=%d points=%d"),
		Team, SpawnPoints.Num());
}

AActor* ASpawnAreaHub::GetRandomSpawnPoint() const
{
	if (SpawnPoints.Num() == 0)
	{
		// 兜底：SpawnPoints 空时返回 SpawnHub 自身 + Z 偏移 100
		// （玩家 spawn 在 SpawnHub 上方 100 单位，掉到 Floor 能站住）
		AActor* Hub = const_cast<ASpawnAreaHub*>(this);
		Hub->SetActorLocation(Hub->GetActorLocation() + FVector(0, 0, 100));
		return Hub;
	}

	const int32 Index = FMath::RandRange(0, SpawnPoints.Num() - 1);
	return SpawnPoints[Index];
}
