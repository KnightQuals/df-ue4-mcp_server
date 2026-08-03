// Copyright Epic Games, Inc. All Rights Reserved.

#include "BattleSectorAnchor.h"
#include "BreakthroughCharacter.h"
#include "Components/TextRenderComponent.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"

void ABattleSectorAnchor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABattleSectorAnchor, OwningTeam);
	DOREPLIFETIME(ABattleSectorAnchor, CaptureProgress);
}

void ABattleSectorAnchor::OnRep_OwningTeam()
{
	// Clients recolor the anchor when the server replicates a new owning team.
	ApplyAnchorColor();
}

// Sets default values
ABattleSectorAnchor::ABattleSectorAnchor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Replicate so clients see the anchor's ownership/color changes in multiplayer.
	bReplicates = true;

	USceneComponent* DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;

	// Box trigger zone (not a sphere) so it doesn't bleed into the SpawnHubs
	// (spawn hubs are 4m away on each side; a 8m sphere would overlap them).
	// BoxExtent = half-size in cm: 300x300x100 = 6m x 6m x 2m box centred on the anchor.
	// SpawnHubs are 4m away (at ±400cm) so a 6m box (radius 3m) still doesn't overlap them.
	CaptureZone = CreateDefaultSubobject<UBoxComponent>(TEXT("CaptureZone"));
	CaptureZone->SetBoxExtent(FVector(800.f, 800.f, 200.f)); // 16m × 16m × 4m (enlarged)
	CaptureZone->SetCollisionProfileName(TEXT("OverlapAll"));
	CaptureZone->SetupAttachment(RootComponent);

	// Floating label "CAPTURE ZONE" above the anchor, recolored on capture.
	AreaLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("AreaLabel"));
	AreaLabel->SetupAttachment(RootComponent);
	AreaLabel->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
	AreaLabel->SetTextRenderColor(FColor(120, 160, 255)); // blue (defender owns by default)
	AreaLabel->SetText(FText::FromString(TEXT("CAPTURE ZONE")));
	AreaLabel->SetWorldSize(100.f); // bigger so it's readable from the player distance
}

// Called when the game starts or when spawned
void ABattleSectorAnchor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("BattleSectorAnchor spawned, team=%d radius=%.0f"), OwningTeam, CaptureRadius);

	// Rotate the label to face the attacker (-X) by default. TextRenderComponent's
	// default normal is +X; a 180° yaw makes the text face -X, so the player walking
	// in from the attacker base sees the text face-on (not mirror-flipped).
	if (AreaLabel)
	{
		AreaLabel->SetWorldRotation(FRotator(0.f, 180.f, 0.f));
	}

	// Bind overlap events. If the Blueprint still holds a stale/invalid CaptureZone
	// (leftover from the C++ SphereComponent → BoxComponent refactor), the component
	// template can deserialize as null/invalid. Rather than depend on the user
	// recompiling the Blueprint, we create a fresh BoxComponent at runtime here so the
	// capture trigger always works regardless of the Blueprint's serialized state.
	if (!CaptureZone || !CaptureZone->IsValidLowLevelFast())
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureZone invalid — creating a runtime BoxComponent trigger."));
		UBoxComponent* NewZone = NewObject<UBoxComponent>(this, TEXT("CaptureZone_Runtime"));
		if (NewZone)
		{
			NewZone->RegisterComponent();
			NewZone->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			NewZone->SetBoxExtent(FVector(800.f, 800.f, 200.f)); // 16m × 16m × 4m (enlarged)
			NewZone->SetCollisionProfileName(TEXT("OverlapAll"));
			CaptureZone = NewZone;
		}
	}

	if (CaptureZone && CaptureZone->IsValidLowLevelFast())
	{
		CaptureZone->OnComponentBeginOverlap.AddDynamic(this, &ABattleSectorAnchor::OnCaptureZoneBeginOverlap);
		CaptureZone->OnComponentEndOverlap.AddDynamic(this, &ABattleSectorAnchor::OnCaptureZoneEndOverlap);
		UE_LOG(LogTemp, Warning, TEXT("CaptureZone overlap events bound OK."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CaptureZone still invalid after runtime creation — capture disabled."));
	}

	// The visible mesh is added in the Blueprint (named "AnchorMesh"). Iterate all
	// StaticMeshComponents on this actor, log each (so we can see what's there), and only
	// create the MID on the one whose name contains "AnchorMesh". Using FindComponentByClass
	// would grab the first StaticMeshComponent, which could be a stray mesh (e.g. a Floor),
	// recoloring the wrong MID.
	TArray<UStaticMeshComponent*> MeshComps;
	GetComponents<UStaticMeshComponent>(MeshComps);

	UE_LOG(LogTemp, Warning, TEXT("Anchor has %d StaticMeshComponent(s):"), MeshComps.Num());

	UStaticMeshComponent* AnchorMeshComp = nullptr;
	for (UStaticMeshComponent* MeshComp : MeshComps)
	{
		if (!MeshComp)
		{
			continue;
		}

		const FString CompName = MeshComp->GetName();
		const FString MatName = MeshComp->GetMaterial(0) ? MeshComp->GetMaterial(0)->GetName() : FString(TEXT("None"));
		UE_LOG(LogTemp, Warning, TEXT("  - mesh '%s' material '%s'"), *CompName, *MatName);

		if (!AnchorMeshComp && CompName.Contains(TEXT("AnchorMesh")))
		{
			AnchorMeshComp = MeshComp;
		}
	}

	if (AnchorMeshComp && AnchorMeshComp->GetMaterial(0))
	{
		AnchorMID = UMaterialInstanceDynamic::Create(AnchorMeshComp->GetMaterial(0), this);
		if (AnchorMID)
		{
			AnchorMeshComp->SetMaterial(0, AnchorMID);
			UE_LOG(LogTemp, Warning, TEXT("AnchorMID created on '%s'"), *AnchorMeshComp->GetName());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AnchorMesh component not found (or has no material) on this BattleSectorAnchor; recolor disabled."));
	}

	ApplyAnchorColor();
}

void ABattleSectorAnchor::OnCaptureZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only the server tracks the in-zone headcount (authoritative capture state).
	if (!HasAuthority())
	{
		return;
	}
	if (!OtherActor)
	{
		return;
	}
	const int32 Team = GetActorTeam(OtherActor);
	if (Team == 0)
	{
		AttackersInZone++;
	}
	else if (Team == 1)
	{
		DefendersInZone++;
	}
	UE_LOG(LogTemp, Warning, TEXT("Actor entered zone (team=%d): attackers=%d defenders=%d"), Team, AttackersInZone, DefendersInZone);
}

void ABattleSectorAnchor::OnCaptureZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// Server-authoritative headcount only.
	if (!HasAuthority())
	{
		return;
	}
	if (!OtherActor)
	{
		return;
	}
	const int32 Team = GetActorTeam(OtherActor);
	if (Team == 0 && AttackersInZone > 0)
	{
		AttackersInZone--;
	}
	else if (Team == 1 && DefendersInZone > 0)
	{
		DefendersInZone--;
	}
	UE_LOG(LogTemp, Warning, TEXT("Actor left zone (team=%d): attackers=%d defenders=%d"), Team, AttackersInZone, DefendersInZone);
}

// Called every frame
void ABattleSectorAnchor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Capture progression is authoritative: only the server advances CaptureProgress
	// and flips OwningTeam. The replicated OwningTeam + OnRep_OwningTeam recolors
	// clients automatically, so we don't run this logic on clients.
	if (!HasAuthority())
	{
		return;
	}

	// Bidirectional capture: progress drifts toward whichever side outnumbers the other.
	const int32 Advantage = AttackersInZone - DefendersInZone;
	if (Advantage > 0)
	{
		// Attackers outnumber -> push toward +1.
		CaptureProgress += CaptureSpeed * DeltaTime;
		if (CaptureProgress >= 1.f)
		{
			CaptureProgress = 1.f;
			if (OwningTeam != 0)
			{
				OwningTeam = 0; // captured by attackers
				UE_LOG(LogTemp, Warning, TEXT("Sector captured by attackers!"));
				ApplyAnchorColor();
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Capture progress %.2f (attackers pushing)"), CaptureProgress);
		}
	}
	else if (Advantage < 0)
	{
		// Defenders outnumber -> push toward -1.
		CaptureProgress -= CaptureSpeed * DeltaTime;
		if (CaptureProgress <= -1.f)
		{
			CaptureProgress = -1.f;
			if (OwningTeam != 1)
			{
				OwningTeam = 1; // held by defenders
				UE_LOG(LogTemp, Warning, TEXT("Sector held by defenders!"));
				ApplyAnchorColor();
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Capture progress %.2f (defenders pushing)"), CaptureProgress);
		}
	}
	// Advantage == 0: contested / empty -> progress frozen.
}

void ABattleSectorAnchor::ApplyAnchorColor()
{
	if (!AnchorMID)
	{
		return;
	}

	// Color reflects current ownership: 0 attacker = red, 1 defender = blue, neutral = white.
	FLinearColor TeamColor = FLinearColor::White; // neutral
	if (OwningTeam == 0) // attackers
	{
		TeamColor = FLinearColor(1.f, 0.f, 0.f); // red
	}
	else if (OwningTeam == 1) // defenders
	{
		TeamColor = FLinearColor(0.f, 0.2f, 1.f); // blue
	}

	// Generic fallback: try common vector parameter names. SetVectorParameterValue on a
	// parameter the material doesn't have is a silent no-op (harmless), so we can probe
	// several names without checking existence. This avoids depending on a single
	// hard-coded parameter name or on reflection APIs that differ across engine versions.
	static const FName ColorParamNames[] =
	{
		FName(TEXT("Color")),
		FName(TEXT("BaseColor")),
		FName(TEXT("Tint")),
		FName(TEXT("Diffuse")),
	};
	for (const FName& ParamName : ColorParamNames)
	{
		AnchorMID->SetVectorParameterValue(ParamName, TeamColor);
	}

	// Also recolor the floating label so the capture state is readable from anywhere.
	if (AreaLabel)
	{
		FColor LabelColor = FColor::White; // neutral
		if (OwningTeam == 0)
		{
			LabelColor = FColor(255, 80, 80); // red = captured by attackers
		}
		else if (OwningTeam == 1)
		{
			LabelColor = FColor(80, 160, 255); // blue = defender owns
		}
		AreaLabel->SetTextRenderColor(LabelColor);
	}
}

int32 ABattleSectorAnchor::GetActorTeam_Implementation(AActor* OtherActor) const
{
	if (!OtherActor)
	{
		return -1;
	}

	// Prefer the real Team property on BreakthroughCharacter (0 = attacker, 1 = defender).
	// This is more reliable than name matching — the player pawn is named
	// "BP_BreakthroughCharacter_C_0" (no "Attacker"/"Defender" substring), so the old
	// name heuristic returned the fallback for the player, which could be wrong.
	if (const ABreakthroughCharacter* Char = Cast<ABreakthroughCharacter>(OtherActor))
	{
		return Char->Team;
	}

	// Fallback: classify by actor name substring for non-character actors.
	const FString Name = OtherActor->GetName();
	if (Name.Contains(TEXT("Attacker")))
	{
		return 0;
	}
	if (Name.Contains(TEXT("Defender")))
	{
		return 1;
	}
	// Unknown actors: treat as attackers (team 0) so the player triggers capture.
	return 0;
}
