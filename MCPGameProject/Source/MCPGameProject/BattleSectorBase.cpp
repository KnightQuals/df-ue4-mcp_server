// Copyright Epic Games, Inc. All Rights Reserved.

#include "BattleSectorBase.h"
#include "BattleCampSector.h"
#include "BattleDefenderCamp.h"
#include "BattleSectorAnchor.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Algo/Sort.h"

// Sets default values
ABattleSectorBase::ABattleSectorBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;
}

// Called when the game starts or when spawned
void ABattleSectorBase::BeginPlay()
{
	Super::BeginPlay();

	InitializeSequentialSectors();
	UE_LOG(LogTemp, Warning, TEXT("BattleSectorBase spawned (sectors=%d duration=%.0f sequential=%s)"),
		Sectors.Num(), MatchDuration, bUseSequentialSectors ? TEXT("true") : TEXT("false"));

	StartMatch();
}

// Called every frame
void ABattleSectorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bMatchOver)
	{
		return;
	}

	// V2 Breakthrough: only the current sector is capturable. A capture unlocks the
	// next sector; capturing the final one ends the match.
	if (bUseSequentialSectors)
	{
		if (UpdateSequentialProgression())
		{
			bMatchOver = true;
			return;
		}
	}
	// Legacy/all-sectors mode: attackers may capture every sector in any order.
	else if (EvaluateWinCondition())
	{
		bMatchOver = true;
		return;
	}

	// Count down remaining time.
	MatchTimeRemaining -= DeltaTime;
	if (MatchTimeRemaining <= 0.f)
	{
		MatchTimeRemaining = 0.f;
		bMatchOver = true;

		// Time's up: attackers win only if they hold every sector; otherwise defenders win.
		Result = (OwningTeamHoldsAllSectors(0)) ? EBattleResult::AttackersWin : EBattleResult::DefendersWin;
		UE_LOG(LogTemp, Warning, TEXT("Match over: %s win"),
			Result == EBattleResult::AttackersWin ? TEXT("attackers") : TEXT("defenders"));
	}
}

void ABattleSectorBase::StartMatch()
{
	MatchTimeRemaining = MatchDuration;
	Result = EBattleResult::InProgress;
	bMatchOver = false;
	UE_LOG(LogTemp, Warning, TEXT("Match started: duration=%.0f sectors=%d"), MatchDuration, Sectors.Num());
}

void ABattleSectorBase::StopMatch()
{
	bMatchOver = true;
	UE_LOG(LogTemp, Warning, TEXT("Match stopped. Result=%s"), *UEnum::GetValueAsString(Result));
}

void ABattleSectorBase::InitializeSequentialSectors()
{
	if (!bUseSequentialSectors)
	{
		return;
	}

	// Blueprint references are optional: discover all placed anchors so MCP-created
	// sectors work without manually wiring an array in Details.
	if (Sectors.Num() == 0)
	{
		for (TActorIterator<ABattleSectorAnchor> It(GetWorld()); It; ++It)
		{
			Sectors.Add(*It);
		}
	}
	Sectors.RemoveAll([](ABattleSectorAnchor* Sector) { return Sector == nullptr; });
	Sectors.Sort([](const ABattleSectorAnchor& A, const ABattleSectorAnchor& B)
	{
		return A.SectorIndex < B.SectorIndex;
	});

	ActiveSectorArrayIndex = 0;
	for (int32 i = 0; i < Sectors.Num(); ++i)
	{
		Sectors[i]->SetSectorActive(i == ActiveSectorArrayIndex);
	}
	if (Sectors.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("V2 Breakthrough initialized: Sector %d active, %d sector(s) total"),
			Sectors[0]->SectorIndex + 1, Sectors.Num());
	}
}

bool ABattleSectorBase::UpdateSequentialProgression()
{
	if (!Sectors.IsValidIndex(ActiveSectorArrayIndex))
	{
		return false;
	}

	ABattleSectorAnchor* CurrentSector = Sectors[ActiveSectorArrayIndex];
	if (!CurrentSector || CurrentSector->OwningTeam != 0)
	{
		return false;
	}

	// Capture committed: freeze the completed objective and unlock the next one.
	CurrentSector->SetSectorActive(false);
	const int32 NextIndex = ActiveSectorArrayIndex + 1;
	if (!Sectors.IsValidIndex(NextIndex))
	{
		Result = EBattleResult::AttackersWin;
		UE_LOG(LogTemp, Warning, TEXT("V2 BREAKTHROUGH COMPLETE: attackers captured final Sector %d"), CurrentSector->SectorIndex + 1);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage((uint64)-1, 6.f, FColor(255, 60, 60),
				TEXT(">>> ATTACKERS WIN - ALL SECTORS CAPTURED! <<<"), true, FVector2D(2.f, 2.f));
		}
		return true;
	}

	ActiveSectorArrayIndex = NextIndex;
	ABattleSectorAnchor* NextSector = Sectors[ActiveSectorArrayIndex];
	NextSector->SetSectorActive(true);
	UE_LOG(LogTemp, Warning, TEXT("V2 BREAKTHROUGH: Sector %d captured; advancing to Sector %d"),
		CurrentSector->SectorIndex + 1, NextSector->SectorIndex + 1);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage((uint64)-1, 5.f, FColor(255, 210, 60),
			FString::Printf(TEXT(">>> SECTOR %d CAPTURED - ADVANCE TO SECTOR %d! <<<"),
				CurrentSector->SectorIndex + 1, NextSector->SectorIndex + 1), true, FVector2D(1.8f, 1.8f));
	}
	return false;
}

bool ABattleSectorBase::OwningTeamHoldsAllSectors(int32 TeamId) const
{
	if (Sectors.Num() == 0)
	{
		return false;
	}

	for (ABattleSectorAnchor* Sector : Sectors)
	{
		if (!Sector || Sector->OwningTeam != TeamId)
		{
			return false;
		}
	}
	return true;
}

bool ABattleSectorBase::EvaluateWinCondition()
{
	// Attackers (team 0) win early if every sector is owned by team 0.
	if (!OwningTeamHoldsAllSectors(0))
	{
		return false;
	}

	Result = EBattleResult::AttackersWin;
	UE_LOG(LogTemp, Warning, TEXT("Match over: attackers win (captured all sectors)"));
	return true;
}
