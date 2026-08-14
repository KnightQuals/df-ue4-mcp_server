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
	AreaLabel->SetText(FText::FromString(AreaName));
	// Playtest verdict: floating labels overlap and mirror-flip — hidden by default.
	AreaLabel->SetVisibility(false);
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
	if (AreaLabel)
	{
		AreaLabel->SetText(FText::FromString(AreaName));
		AreaLabel->SetTextRenderColor(BoundaryColor.ToFColor(true));
		AreaLabel->SetVisibility(bShowLabel);
	}
	RefreshBoundaryVisuals();
}

void ABattleAreaSpline::ConfigureGameplayArea(const FString& InAreaName, const FLinearColor& InBoundaryColor,
	float HalfExtentX, float HalfExtentY)
{
	AreaName = InAreaName;
	BoundaryColor = InBoundaryColor;
	bSafeArea = false; // display region only; the outer safe spline owns forbidden-zone logic.
	ConfigureRectangle(HalfExtentX, HalfExtentY);
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
	// Material path was wrong ("/Game/Materials/..."), which left the boundary grey.
	// The asset actually lives under /Game/Blueprints; keep the old path as fallback.
	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Blueprints/M_AnchorColor.M_AnchorColor"));
	if (!BaseMaterial)
	{
		BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AnchorColor.M_AnchorColor"));
	}
	if (!CubeMesh)
	{
		return;
	}

	// Fully flush with the ground (user direction): zero lift, zero height — the
	// boundary reads as a painted line and can never feel like an obstacle.
	const FVector LineLift(0.f, 0.f, 0.f);
	const int32 Count = BoundarySpline->GetNumberOfSplinePoints();
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Next = (i + 1) % Count;
		USplineMeshComponent* Mesh = NewObject<USplineMeshComponent>(this, *FString::Printf(TEXT("SafeBoundarySegment_%d"), i));
		Mesh->SetStaticMesh(CubeMesh);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetForwardAxis(ESplineMeshAxis::X);
		// SnapToTarget: relative transform stays identity so the local Start/End
		// coordinates map 1:1 into spline space. KeepRelativeTransform bakes a
		// -ActorLocation offset into the not-yet-registered component, which shifted
		// non-origin area outlines (bases/sectors) onto the map centre.
		Mesh->AttachToComponent(BoundarySpline, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		const FVector Start = BoundarySpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local) + LineLift;
		const FVector End = BoundarySpline->GetLocationAtSplinePoint(Next, ESplineCoordinateSpace::Local) + LineLift;
		const FVector Tangent = (End - Start).GetSafeNormal() * (End - Start).Size();
		Mesh->SetStartAndEnd(Start, Tangent, End, Tangent, true);
		// Cube is 100cm: 12cm wide, 0.5cm tall — visually zero height (exact 0 gives
		// degenerate normals and z-fighting flicker on the floor).
		Mesh->SetStartScale(FVector2D(0.12f, 0.005f), true);
		Mesh->SetEndScale(FVector2D(0.12f, 0.005f), true);
		if (BaseMaterial)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			if (MID)
			{
				MID->SetVectorParameterValue(TEXT("Color"), BoundaryColor);
				MID->SetVectorParameterValue(TEXT("BaseColor"), BoundaryColor);
				Mesh->SetMaterial(0, MID);
			}
		}
		Mesh->RegisterComponent();
		// Diagnostics: log the RENDERED world-space bounds (not the intended spline
		// coords) so attach/transform bugs are visible in the output log directly.
		Mesh->UpdateBounds();
		const FBox SegBounds = Mesh->Bounds.GetBox();
		UE_LOG(LogTemp, Warning, TEXT("AreaSpline[%s] seg%d world bounds X[%.0f,%.0f] Y[%.0f,%.0f] (actor at %s)"),
			*AreaName, i, SegBounds.Min.X, SegBounds.Max.X, SegBounds.Min.Y, SegBounds.Max.Y,
			*GetActorLocation().ToCompactString());
		BoundaryMeshes.Add(Mesh);
	}
}
