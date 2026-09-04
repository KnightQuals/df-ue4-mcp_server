// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLItemPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Net/UnrealNetwork.h"

ASOLItemPickup::ASOLItemPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	// Replication opt-in (see SOLContainer.cpp for why). Without bReplicates
	// the Data ReplicatedUsing field stays empty on every client.
	bReplicates = true;
	NetUpdateFrequency = 30.f;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->InitSphereRadius(150.f);
	PickupSphere->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	RootComponent = PickupSphere;

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(RootComponent);
	ItemMesh->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'"));
	if (CubeMesh.Succeeded())
	{
		ItemMesh->SetStaticMesh(CubeMesh.Object);
	}

	LabelComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelComponent->SetupAttachment(RootComponent);
	LabelComponent->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	LabelComponent->SetHorizontalAlignment(EHTA_Center);
	LabelComponent->SetTextRenderColor(FColor::Yellow);
	LabelComponent->SetWorldSize(24.f);
}

ASOLItemPickup* ASOLItemPickup::SpawnGrounded(UWorld* World, const FVector& ApproxLocation, const FSOLItemInstance& InData)
{
	if (!World)
	{
		return nullptr;
	}

	// Trace from above the requested spot downwards; wherever the first
	// blocking surface is becomes the resting place. 12 cm lift keeps the
	// cube visually on the surface instead of half-sunken.
	FVector Where = ApproxLocation;
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SOLPickupGround), false);
	const FVector TraceStart = ApproxLocation + FVector(0.f, 0.f, 150.f);
	const FVector TraceEnd = ApproxLocation - FVector(0.f, 0.f, 500.f);
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		Where = Hit.ImpactPoint + FVector(0.f, 0.f, 12.f);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASOLItemPickup* Pickup = World->SpawnActor<ASOLItemPickup>(ASOLItemPickup::StaticClass(), Where, FRotator::ZeroRotator, Params);
	if (Pickup)
	{
		Pickup->Data = InData;
		Pickup->RefreshLabel();
	}
	return Pickup;
}

void ASOLItemPickup::RefreshLabel()
{
	// ASCII only — TextRender has no CJK glyph fallback.
	const FString Label = FString::Printf(TEXT("%s x%d"), *Data.Def.ItemID.ToString(), Data.Count);
	LabelComponent->SetText(FText::FromString(Label));
}

void ASOLItemPickup::OnRep_Data()
{
	// The server assigned Data over replication — rebuild the world label
	// on this client. (On the server this never fires; it calls
	// RefreshLabel() directly at spawn time.)
	RefreshLabel();
}

void ASOLItemPickup::PostLoad()
{
	Super::PostLoad();

	// A constructor only runs on creation; a map-loaded instance restores
	// its serialised archive afterwards, which would revert these flags.
	bReplicates = true;
	NetDormancy = DORM_Never;
	NetUpdateFrequency = 30.f;
}

void ASOLItemPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASOLItemPickup, Data);
}
