// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLExtractionZone.h"
#include "SOL.h"
#include "SOLCharacter.h"
#include "SOLPlayerController.h"
#include "SOLPlayerState.h"
#include "SOLGameState.h"
#include "SOLBackpackComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

ASOLExtractionZone::ASOLExtractionZone()
{
	// No Tick: evaluation runs on a 10Hz timer started in BeginPlay. Keeping
	// bCanEverTick false also avoids the UE4.27 editor crash where recompiling
	// a tickable blueprint with live instances calls into a destroyed tick
	// function (LowLevelFatalError in EngineBaseTypes.h, hit during MCP
	// provisioning of this very class).
	PrimaryActorTick.bCanEverTick = false;

	// Replication opt-in: the same flag whose absence made container loot
	// invisible to remote clients. Progress/Radius/HoldSeconds all need to
	// reach every client for the HUD ring to be drawable.
	bReplicates = true;
	NetDormancy = DORM_Never;
	NetUpdateFrequency = 30.f;

	// A flat disc marking the footprint. Engine primitives only — the whole
	// project stays zero-art-asset on purpose (nothing to import, nothing to
	// break when the map moves).
	ZoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMesh"));
	ZoneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = ZoneMesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("StaticMesh'/Engine/BasicShapes/Cylinder.Cylinder'"));
	if (CylinderMesh.Succeeded())
	{
		ZoneMesh->SetStaticMesh(CylinderMesh.Object);
	}

	// A tall thin pillar so the zone reads from a distance even without a
	// minimap. Placed at the centre, above the disc.
	BeaconMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeaconMesh"));
	BeaconMesh->SetupAttachment(RootComponent);
	BeaconMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeaconCylinder(TEXT("StaticMesh'/Engine/BasicShapes/Cylinder.Cylinder'"));
	if (BeaconCylinder.Succeeded())
	{
		BeaconMesh->SetStaticMesh(BeaconCylinder.Object);
	}
}

void ASOLExtractionZone::PostLoad()
{
	Super::PostLoad();

	// Placed instances predate replication on this class; the serialised
	// archive would otherwise reinstate bReplicates=false (see
	// ASOLContainer::PostLoad for the full story).
	bReplicates = true;
	NetDormancy = DORM_Never;
	NetUpdateFrequency = 30.f;
}

void ASOLExtractionZone::BeginPlay()
{
	Super::BeginPlay();

	// Config lookup runs on both server and clients: the tuning values are
	// static data (radius / hold time), so each side loads them from its own
	// copy of the DataTable instead of spending bandwidth replicating them.
	// Replication of these fields is kept anyway as a safety net for
	// MCP-spawned zones whose row may be missing on one side.
	TryLoadConfigRow();
	SyncVisualScale();

	// Only the server evaluates the hold: clients read the replicated
	// Progress array. Starting the timer server-side only also means a
	// client never even schedules work it has no authority to do.
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(EvalTimerHandle, this,
			&ASOLExtractionZone::EvaluateZone, EvalInterval, true);
	}

	UE_LOG(LogSOL, Log, TEXT("ExtractionZone %s ready: r=%.0fcm hold=%.1fs x%.2f minValue=%d (%s)"),
		*GetName(), Radius, HoldSeconds, ValueMultiplier, MinValueRequired, *GetDisplayName());
}

bool ASOLExtractionZone::TryLoadConfigRow()
{
	if (ConfigTablePath.IsEmpty())
	{
		return false;
	}

	UDataTable* Table = LoadObject<UDataTable>(nullptr, *ConfigTablePath);
	if (!Table)
	{
		return false;
	}

	// Row key defaults to the actor name, so MCP can spawn a zone and have
	// it self-configure with zero property writes (array/struct properties
	// do not travel over the reflection setter anyway).
	FName RowKey = ConfigRow.IsNone() ? FName(*GetName()) : ConfigRow;
	FSOLExtractionRow* Row = Table->FindRow<FSOLExtractionRow>(RowKey, TEXT("SOLExtractionZone"), false);

	// Retry without the trailing spawn index (SOL_Extract_1 -> SOL_Extract):
	// FName splits trailing digits into its number part, so a hand-authored
	// row may only carry the stem. Same trick the container loader needed.
	if (!Row)
	{
		FString Stem = GetName();
		int32 UnderscoreIdx = INDEX_NONE;
		if (Stem.FindLastChar('_', UnderscoreIdx) && UnderscoreIdx > 0)
		{
			const FString Tail = Stem.Mid(UnderscoreIdx + 1);
			if (Tail.IsNumeric())
			{
				Stem = Stem.Left(UnderscoreIdx);
				Row = Table->FindRow<FSOLExtractionRow>(FName(*Stem), TEXT("SOLExtractionZone"), false);
				if (Row)
				{
					RowKey = FName(*Stem);
				}
			}
		}
	}

	if (!Row)
	{
		UE_LOG(LogSOL, Log, TEXT("ExtractionZone %s: no config row, using defaults"), *GetName());
		return false;
	}

	if (!Row->DisplayNameZh.IsEmpty())
	{
		DisplayNameZh = Row->DisplayNameZh;
	}
	Radius = Row->Radius > 0.f ? Row->Radius : Radius;
	HoldSeconds = Row->HoldSeconds > 0.f ? Row->HoldSeconds : HoldSeconds;
	ValueMultiplier = Row->ValueMultiplier > 0.f ? Row->ValueMultiplier : ValueMultiplier;
	MinValueRequired = Row->MinValueRequired;

	UE_LOG(LogSOL, Log, TEXT("ExtractionZone %s: loaded row '%s' (zh=%s)"),
		*GetName(), *RowKey.ToString(), *DisplayNameZh);
	return true;
}

void ASOLExtractionZone::SyncVisualScale()
{
	// Engine Cylinder is 100cm diameter / 100cm tall at scale 1, pivot centre.
	if (ZoneMesh)
	{
		const float DiscScale = (Radius * 2.f) / 100.f;
		ZoneMesh->SetRelativeScale3D(FVector(DiscScale, DiscScale, 0.06f));
	}
	if (BeaconMesh)
	{
		// Thin 8m pillar rising from the disc centre.
		BeaconMesh->SetRelativeScale3D(FVector(0.5f / FMath::Max(ZoneMesh ? ZoneMesh->GetRelativeScale3D().X : 1.f, KINDA_SMALL_NUMBER),
			0.5f / FMath::Max(ZoneMesh ? ZoneMesh->GetRelativeScale3D().Y : 1.f, KINDA_SMALL_NUMBER),
			8.f / FMath::Max(ZoneMesh ? ZoneMesh->GetRelativeScale3D().Z : 1.f, KINDA_SMALL_NUMBER)));
		BeaconMesh->SetRelativeLocation(FVector(0.f, 0.f, 400.f / FMath::Max(ZoneMesh ? ZoneMesh->GetRelativeScale3D().Z : 1.f, KINDA_SMALL_NUMBER)));
	}
}

bool ASOLExtractionZone::IsPawnInside(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}
	// Column test: horizontal distance only. A player standing on a rock or
	// crouched in a ditch inside the disc still counts, which is what the
	// visual (a flat disc on the ground) promises.
	const float DistSq = FVector::DistSquared2D(Pawn->GetActorLocation(), GetActorLocation());
	return DistSq <= Radius * Radius;
}

float ASOLExtractionZone::GetProgressFor(const APlayerState* Player) const
{
	if (!Player || HoldSeconds <= 0.f)
	{
		return 0.f;
	}
	for (const FSOLExtractionProgress& Entry : Progress)
	{
		if (Entry.Player == Player)
		{
			return FMath::Clamp(Entry.Elapsed / HoldSeconds, 0.f, 1.f);
		}
	}
	return 0.f;
}

void ASOLExtractionZone::EvaluateZone()
{
	// Server authoritative timer. Clients read the replicated Progress array
	// and never advance it — a tampered client cannot extract itself.
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Fixed step: the timer fires at EvalInterval, so accumulating that value
	// keeps the hold duration accurate regardless of frame rate (and unlike a
	// Tick delta, it does not stretch during a hitch).
	const float DeltaSeconds = EvalInterval;

	// Track which entries we touched this frame; anything untouched means the
	// player left (or died / extracted) and loses all progress.
	TArray<APlayerState*> SeenThisFrame;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		ASOLPlayerController* PC = Cast<ASOLPlayerController>(It->Get());
		if (!PC)
		{
			continue;
		}
		ASOLPlayerState* PS = PC->GetPlayerState<ASOLPlayerState>();
		APawn* Pawn = PC->GetPawn();
		if (!PS || !Pawn || PS->bExtracted)
		{
			continue; // already out of the round
		}
		if (!IsPawnInside(Pawn))
		{
			continue;
		}

		// Gate on carried value: a zone can demand you actually bring
		// something back instead of just walking out.
		if (MinValueRequired > 0)
		{
			const USOLBackpackComponent* Backpack = Pawn->FindComponentByClass<USOLBackpackComponent>();
			const int32 Carried = Backpack ? Backpack->CurrentValue() : 0;
			if (Carried < MinValueRequired)
			{
				// Log once per entry attempt rather than at 10Hz: the HUD
				// already tells the player, this line is for verification.
				if (!IneligibleReported.Contains(PS))
				{
					IneligibleReported.Add(PS);
					UE_LOG(LogSOL, Log, TEXT("ExtractionZone %s: %s ineligible (carries %d, needs %d)"),
						*GetDisplayName(), *PS->GetPlayerName(), Carried, MinValueRequired);
					PC->ClientShowMessage(FString::Printf(TEXT("%s 需要携带价值 %d（当前 %d）"),
						*GetDisplayName(), MinValueRequired, Carried));
				}
				continue; // standing inside but not eligible; no progress
			}
			// Became eligible (looted more and came back): allow a fresh
			// notification if they leave and return under-value again.
			IneligibleReported.Remove(PS);
		}

		SeenThisFrame.Add(PS);

		// Find or create this player's progress entry.
		// Advance by index, not by a cached pointer: CompleteExtraction below
		// mutates Progress (RemoveAll), which would leave a raw element
		// pointer dangling. Indices survive the same mutation pattern here
		// because completion is handled after the write.
		int32 EntryIdx = Progress.IndexOfByPredicate(
			[PS](const FSOLExtractionProgress& E) { return E.Player == PS; });
		if (EntryIdx == INDEX_NONE)
		{
			FSOLExtractionProgress NewEntry;
			NewEntry.Player = PS;
			NewEntry.Elapsed = 0.f;
			EntryIdx = Progress.Add(NewEntry);

			UE_LOG(LogSOL, Log, TEXT("ExtractionZone %s: %s started extracting"),
				*GetDisplayName(), *PS->GetPlayerName());
			PC->ClientShowMessage(FString::Printf(TEXT("正在撤离：%s（保持在区域内 %.0f 秒）"),
				*GetDisplayName(), HoldSeconds));
		}

		Progress[EntryIdx].Elapsed += DeltaSeconds;
		if (Progress[EntryIdx].Elapsed >= HoldSeconds)
		{
			CompleteExtraction(PC);
			// CompleteExtraction removed this player's entry already; keeping
			// it out of SeenThisFrame stops the reset pass from double-logging.
			SeenThisFrame.Remove(PS);
			IneligibleReported.Remove(PS);
		}
	}

	// Leaving the zone wipes progress: an extraction is a commitment, not a
	// checkpoint. Iterate backwards so removal stays index-safe.
	for (int32 Idx = Progress.Num() - 1; Idx >= 0; --Idx)
	{
		const FSOLExtractionProgress& Entry = Progress[Idx];
		if (!Entry.Player || !SeenThisFrame.Contains(Entry.Player))
		{
			if (Entry.Player)
			{
				UE_LOG(LogSOL, Log, TEXT("ExtractionZone %s: %s left, progress reset (%.1fs lost)"),
					*GetDisplayName(), *Entry.Player->GetPlayerName(), Entry.Elapsed);
				// Allow the "not enough loot" hint to fire again next approach.
				IneligibleReported.Remove(Entry.Player);
			}
			Progress.RemoveAt(Idx);
		}
	}

	// Anyone who walked away entirely (not even standing inside) also clears
	// their hint state, so returning later gets fresh feedback.
	for (auto ReportedIt = IneligibleReported.CreateIterator(); ReportedIt; ++ReportedIt)
	{
		APlayerState* Reported = *ReportedIt;
		if (!Reported || !SeenThisFrame.Contains(Reported))
		{
			// Still inside but under-value players are not in SeenThisFrame
			// (they were skipped), so verify by position before clearing.
			bool bStillInside = false;
			for (FConstPlayerControllerIterator It2 = World->GetPlayerControllerIterator(); It2; ++It2)
			{
				const APlayerController* PC2 = It2->Get();
				if (PC2 && PC2->PlayerState == Reported && IsPawnInside(PC2->GetPawn()))
				{
					bStillInside = true;
					break;
				}
			}
			if (!bStillInside)
			{
				ReportedIt.RemoveCurrent();
			}
		}
	}
}

void ASOLExtractionZone::CompleteExtraction(ASOLPlayerController* PC)
{
	if (!PC || !HasAuthority())
	{
		return;
	}
	ASOLPlayerState* PS = PC->GetPlayerState<ASOLPlayerState>();
	APawn* Pawn = PC->GetPawn();
	if (!PS || !Pawn)
	{
		return;
	}

	USOLBackpackComponent* Backpack = Pawn->FindComponentByClass<USOLBackpackComponent>();
	const int32 RawValue = Backpack ? Backpack->CurrentValue() : 0;
	const int32 Stacks = Backpack ? Backpack->Items.Num() : 0;
	const int32 Payout = FMath::RoundToInt(RawValue * ValueMultiplier);

	// Bank the result on the PlayerState (replicated to everyone) and clear
	// the backpack: the loot has left the map with the player.
	PS->MarkExtracted(Payout, Stacks, GetDisplayName());
	if (Backpack)
	{
		// ClearAll re-applies the encumbrance speed itself, so "Items changed
		// => speed reapplied" stays unconditional. A raw Items.Reset() used to
		// bypass it — harmless while extraction destroys the pawn immediately
		// below, but a live trap the moment that changes.
		Backpack->ClearAll();
	}

	// Drop the progress entry so the ring disappears for every client.
	Progress.RemoveAll([PS](const FSOLExtractionProgress& E) { return E.Player == PS; });

	PC->ClientShowMessage(FString::Printf(TEXT("撤离成功！收益 %d（%d 件物资）"), Payout, Stacks));

	// Close any loot panel the player left open. The settlement board hides it
	// visually, but HUD->OpenedContainer would stay set, so a stray F press
	// after extracting would still be interpreted as "close the container".
	PC->ClientClearContainerPanel();

	// Update the round summary (extracted count / team value / round-over).
	// Event-driven on purpose: recounting once per settlement beats polling
	// the player array every tick.
	if (ASOLGameState* GS = GetWorld() ? GetWorld()->GetGameState<ASOLGameState>() : nullptr)
	{
		GS->RefreshRoundState();
	}

	// Remove the pawn from the world: the round is over for this player.
	// The PlayerState (and therefore the settlement screen) survives.
	Pawn->Destroy();
}

void ASOLExtractionZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASOLExtractionZone, Progress);
	DOREPLIFETIME(ASOLExtractionZone, Radius);
	DOREPLIFETIME(ASOLExtractionZone, HoldSeconds);
	DOREPLIFETIME(ASOLExtractionZone, ValueMultiplier);
	DOREPLIFETIME(ASOLExtractionZone, MinValueRequired);
}
