// Copyright Epic Games, Inc. All Rights Reserved.

#include "BattleAreaSpline.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABattleAreaSpline::ABattleAreaSpline()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	BoundarySpline = CreateDefaultSubobject<USplineComponent>(TEXT("SafeBoundarySpline"));
	RootComponent = BoundarySpline;
	BoundarySpline->SetClosedLoop(true);

	AreaLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SafeAreaLabel"));
	AreaLabel->SetupAttachment(RootComponent);
	AreaLabel->SetRelativeLocation(FVector(0.f, 0.f, 260.f));
	AreaLabel->SetWorldSize(90.f);
	AreaLabel->SetTextRenderColor(FColor(90, 230, 130));
	AreaLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	AreaLabel->SetText(FText::FromString(TEXT("SAFE BATTLEFIELD")));
}

void ABattleAreaSpline::BeginPlay()
{
	Super::BeginPlay();
	RefreshBoundaryVisuals();
}

void ABattleAreaSpline::ConfigureRectangle(float HalfExtentX, float HalfExtentY)
{
	if (!BoundarySpline)
	{
		return;
	}

	BoundarySpline->ClearSplinePoints(false);
	const TArray<FVector> Points = {
		FVector(-HalfExtentX, -HalfExtentY, 0.f),
		FVector(HalfExtentX, -HalfExtentY, 0.f),
		FVector(HalfExtentX, HalfExtentY, 0.f),
		FVector(-HalfExtentX, HalfExtentY, 0.f)
	};
	for (const FVector& Point : Points)
	{
		BoundarySpline->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
	}
	BoundarySpline->SetClosedLoop(true, false);
	BoundarySpline->UpdateSpline();
	RefreshBoundaryVisuals();
}

bool ABattleAreaSpline::IsPointInside(const FVector& WorldPoint) const
{
	if (!BoundarySpline || !BoundarySpline->IsClosedLoop() || BoundarySpline->GetNumberOfSplinePoints() < 3)
	{
		return false;
	}

	// Standard 2D ray-casting test against the spline control polygon. For the initial
	// rectangle this is exact; designers can reshape control points into any convex or
	// concave safe battlefield boundary later without changing character logic.
	const FVector LocalPoint = BoundarySpline->GetComponentTransform().InverseTransformPosition(WorldPoint);
	const int32 Count = BoundarySpline->GetNumberOfSplinePoints();
	bool bInside = false;
	for (int32 i = 0, j = Count - 1; i < Count; j = i++)
	{
		const FVector A = BoundarySpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
		const FVector B = BoundarySpline->GetLocationAtSplinePoint(j, ESplineCoordinateSpace::Local);
		const bool bCrossesY = ((A.Y > LocalPoint.Y) != (B.Y > LocalPoint.Y));
		if (bCrossesY)
		{
			const float IntersectX = (B.X - A.X) * (LocalPoint.Y - A.Y) / (B.Y - A.Y) + A.X;
			if (LocalPoint.X < IntersectX)
			{
				bInside = !bInside;
			}
		}
	}
	return bInside;
}

void ABattleAreaSpline::RefreshBoundaryVisuals()
{
	if (!BoundarySpline || !GetWorld())
	{
		return;
	}
	for (USplineMeshComponent* Mesh : BoundaryMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	BoundaryMeshes.Empty();

	// Runtime-safe asset load (FObjectFinder is constructor-only and would fatal here).
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AnchorColor.M_AnchorColor"));
	if (!CubeMesh)
	{
		return;
	}

	const int32 Count = BoundarySpline->GetNumberOfSplinePoints();
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Next = (i + 1) % Count;
		USplineMeshComponent* Mesh = NewObject<USplineMeshComponent>(this, *FString::Printf(TEXT("SafeBoundarySegment_%d"), i));
		Mesh->SetStaticMesh(CubeMesh);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetForwardAxis(ESplineMeshAxis::X);
		Mesh->AttachToComponent(BoundarySpline, FAttachmentTransformRules::KeepRelativeTransform);
		const FVector Start = BoundarySpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
		const FVector End = BoundarySpline->GetLocationAtSplinePoint(Next, ESplineCoordinateSpace::Local);
		const FVector Tangent = (End - Start).GetSafeNormal() * (End - Start).Size();
		Mesh->SetStartAndEnd(Start, Tangent, End, Tangent, true);
		Mesh->SetStartScale(FVector2D(0.08f, 0.06f), true);
		Mesh->SetEndScale(FVector2D(0.08f, 0.06f), true);
		if (BaseMaterial)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			if (MID)
			{
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.08f, 0.9f, 0.2f));
				MID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.08f, 0.9f, 0.2f));
				Mesh->SetMaterial(0, MID);
			}
		}
		Mesh->RegisterComponent();
		BoundaryMeshes.Add(Mesh);
	}
}
