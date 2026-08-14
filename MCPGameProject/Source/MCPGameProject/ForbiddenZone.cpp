// Copyright Epic Games, Inc. All Rights Reserved.

#include "ForbiddenZone.h"
#include "BreakthroughCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

AForbiddenZone::AForbiddenZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ForbiddenTrigger"));
	RootComponent = TriggerBox;
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);

	WarningLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ForbiddenWarning"));
	WarningLabel->SetupAttachment(RootComponent);
	WarningLabel->SetRelativeLocation(FVector(0.f, 0.f, 220.f));
	WarningLabel->SetWorldSize(72.f);
	WarningLabel->SetTextRenderColor(FColor(255, 80, 60));
	WarningLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	WarningLabel->SetText(FText::FromString(TEXT("FORBIDDEN ZONE")));

	// Visible V2 boundary: thin red cube bar along each trigger strip. It has no
	// collision, so the player can enter the zone and receive the intended countdown.
	BorderVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ForbiddenBorderVisual"));
	BorderVisual->SetupAttachment(RootComponent);
	BorderVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BorderVisual->SetRelativeLocation(FVector(0.f, 0.f, -70.f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BorderVisual->SetStaticMesh(CubeMesh.Object);
	}
	// Asset lives under /Game/Blueprints; the old /Game/Materials path silently failed
	// and left the perimeter strips grey instead of red.
	UMaterialInterface* BorderMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Blueprints/M_AnchorColor.M_AnchorColor"));
	if (!BorderMaterial)
	{
		BorderMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AnchorColor.M_AnchorColor"));
	}
	if (BorderMaterial)
	{
		// Only assign the base material in the constructor. Creating a dynamic material
		// here uses NewObject during default-subobject construction and fatals in UE4.27;
		// BeginPlay creates/tints the MID safely after the actor exists.
		BorderVisual->SetMaterial(0, BorderMaterial);
	}
}

void AForbiddenZone::ConfigureZone(const FString& InZoneLabel, const FVector& InExtent)
{
	ZoneLabel = InZoneLabel;
	if (TriggerBox)
	{
		TriggerBox->SetBoxExtent(InExtent);
	}
	if (WarningLabel)
	{
		WarningLabel->SetText(FText::FromString(ZoneLabel));
	}
	if (BorderVisual)
	{
		// Engine BasicShapes/Cube is 100cm wide, hence extent / 50 gives the scale.
		// Keep Z thin so this reads as a red perimeter line rather than a wall.
		BorderVisual->SetRelativeScale3D(FVector(InExtent.X / 50.f, InExtent.Y / 50.f, 0.08f));
	}
}

void AForbiddenZone::BeginPlay()
{
	Super::BeginPlay();
	if (BorderVisual && BorderVisual->GetMaterial(0))
	{
		UMaterialInstanceDynamic* BorderMID = UMaterialInstanceDynamic::Create(BorderVisual->GetMaterial(0), this);
		if (BorderMID)
		{
			BorderMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.04f, 0.02f));
			BorderMID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(1.f, 0.04f, 0.02f));
			BorderVisual->SetMaterial(0, BorderMID);
		}
	}
	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AForbiddenZone::OnZoneBeginOverlap);
		TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AForbiddenZone::OnZoneEndOverlap);
	}
}

void AForbiddenZone::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}
	if (ABreakthroughCharacter* Character = Cast<ABreakthroughCharacter>(OtherActor))
	{
		Character->SetForbiddenZoneOverlap(true, ZoneLabel);
	}
}

void AForbiddenZone::OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority())
	{
		return;
	}
	if (ABreakthroughCharacter* Character = Cast<ABreakthroughCharacter>(OtherActor))
	{
		Character->SetForbiddenZoneOverlap(false, ZoneLabel);
	}
}
