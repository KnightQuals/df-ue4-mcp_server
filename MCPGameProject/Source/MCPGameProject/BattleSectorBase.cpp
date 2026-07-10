// Copyright Epic Games, Inc. All Rights Reserved.

#include "BattleSectorBase.h"
#include "BattleCampSector.h"
#include "BattleDefenderCamp.h"
#include "BattleSectorAnchor.h"

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

	UE_LOG(LogTemp, Warning, TEXT("BattleSectorBase spawned (sectors=%d duration=%.0f)"), Sectors.Num(), MatchDuration);

	// Auto-start the match on play for convenience.
	StartMatch();
}

// Called every frame
void ABattleSectorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bMatchActive)
	{
		return;
	}

	// Count down remaining time.
	RemainingTime -= DeltaTime;
	if (RemainingTime <= 0.f)
	{
		RemainingTime = 0.f;
		bMatchActive = false;

		// Time exhausted: if attackers have not captured every sector, defenders win.
		bool bAllCapturedByAttackers = true;
		for (ABattleSectorAnchor* Sector : Sectors)
		{
			if (Sector && Sector->OwningTeam != 0)
			{
				bAllCapturedByAttackers = false;
				break;
			}
		}

		Result = bAllCapturedByAttackers ? EBattleResult::AttackersWin : EBattleResult::DefendersWin;
		UE_LOG(LogTemp, Warning, TEXT("Match time over. Result=%s"),
			*UEnum::GetValueAsString(Result));
		return;
	}

	// Mid-match: check if attackers already captured every sector.
	if (EvaluateWinCondition())
	{
		bMatchActive = false;
	}
}

void ABattleSectorBase::StartMatch()
{
	RemainingTime = MatchDuration;
	Result = EBattleResult::InProgress;
	bMatchActive = true;
	UE_LOG(LogTemp, Warning, TEXT("Match started: duration=%.0f sectors=%d"), MatchDuration, Sectors.Num());
}

void ABattleSectorBase::StopMatch()
{
	bMatchActive = false;
	UE_LOG(LogTemp, Warning, TEXT("Match stopped. Result=%s"), *UEnum::GetValueAsString(Result));
}

bool ABattleSectorBase::EvaluateWinCondition()
{
	// Attackers win early if every sector is owned by team 0.
	if (Sectors.Num() == 0)
	{
		return false;
	}

	for (ABattleSectorAnchor* Sector : Sectors)
	{
		if (!Sector || Sector->OwningTeam != 0)
		{
			return false; // at least one sector not yet captured by attackers
		}
	}

	Result = EBattleResult::AttackersWin;
	UE_LOG(LogTemp, Warning, TEXT("Attackers captured all sectors! Result=%s"),
		*UEnum::GetValueAsString(Result));
	return true;
}
