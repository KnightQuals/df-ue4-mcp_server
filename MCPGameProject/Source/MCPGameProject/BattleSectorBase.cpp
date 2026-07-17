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

	// Early win check: attackers may capture every sector before time runs out.
	if (EvaluateWinCondition())
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
