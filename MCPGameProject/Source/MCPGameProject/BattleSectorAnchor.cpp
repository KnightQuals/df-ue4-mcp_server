// Copyright Epic Games, Inc. All Rights Reserved.

#include "BattleSectorAnchor.h"

// Sets default values
ABattleSectorAnchor::ABattleSectorAnchor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;

	CaptureZone = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureZone"));
	CaptureZone->SetSphereRadius(CaptureRadius);
	CaptureZone->SetCollisionProfileName(TEXT("OverlapAll"));
	CaptureZone->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void ABattleSectorAnchor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("BattleSectorAnchor spawned, team=%d radius=%.0f"), OwningTeam, CaptureRadius);

	CaptureZone->OnComponentBeginOverlap.AddDynamic(this, &ABattleSectorAnchor::OnCaptureZoneBeginOverlap);
	CaptureZone->OnComponentEndOverlap.AddDynamic(this, &ABattleSectorAnchor::OnCaptureZoneEndOverlap);
}

void ABattleSectorAnchor::OnCaptureZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AttackersInZone++;
	UE_LOG(LogTemp, Warning, TEXT("Actor entered zone, attackers=%d"), AttackersInZone);
}

void ABattleSectorAnchor::OnCaptureZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (AttackersInZone > 0)
	{
		AttackersInZone--;
	}
	UE_LOG(LogTemp, Warning, TEXT("Actor left zone, attackers=%d"), AttackersInZone);
}

// Called every frame
void ABattleSectorAnchor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (AttackersInZone > 0 && OwningTeam != 1)
	{
		CaptureProgress += CaptureSpeed * DeltaTime;
		if (CaptureProgress >= 100.f)
		{
			CaptureProgress = 100.f;
			OwningTeam = 1;
			UE_LOG(LogTemp, Warning, TEXT("Captured by attackers!"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Capture progress %.1f"), CaptureProgress);
		}
	}
}
