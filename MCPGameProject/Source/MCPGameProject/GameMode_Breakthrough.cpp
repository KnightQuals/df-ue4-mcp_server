// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameMode_Breakthrough.h"
#include "SpawnAreaHub.h"
#include "BreakthroughCharacter.h"
#include "BattleSectorAnchor.h"
#include "DefaultPlayerController.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"

AGameMode_Breakthrough::AGameMode_Breakthrough()
{
	DefaultPawnClass = ABreakthroughCharacter::StaticClass();
	PlayerControllerClass = ADefaultPlayerController::StaticClass();
}

void AGameMode_Breakthrough::BeginPlay()
{
	Super::BeginPlay();

	// Find attacker + defender spawn hubs by Team UPROPERTY.
	for (TActorIterator<ASpawnAreaHub> It(GetWorld()); It; ++It)
	{
		ASpawnAreaHub* Hub = *It;
		if (Hub->Team == 0 && !AttackerHub)
		{
			AttackerHub = Hub;
		}
		else if (Hub->Team == 1 && !DefenderHub)
		{
			DefenderHub = Hub;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("GameMode_Breakthrough started, AttackerHub=%s DefenderHub=%s"),
		AttackerHub ? *AttackerHub->GetName() : TEXT("None"),
		DefenderHub ? *DefenderHub->GetName() : TEXT("None"));
}

APawn* AGameMode_Breakthrough::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	if (!NewPlayer)
	{
		return nullptr;
	}

	// Alternate team assignment: even spawn index = attacker (team 0), odd = defender (team 1).
	const int32 Team = SpawnedPawnCount % 2;
	SpawnedPawnCount++;

	TSubclassOf<ABreakthroughCharacter> PawnClass = (Team == 0) ? AttackerClass : DefenderClass;
	if (!PawnClass)
	{
		PawnClass = ABreakthroughCharacter::StaticClass();
	}

	// Pick a spawn transform: prefer the matching team's spawn hub, fall back to StartSpot.
	ASpawnAreaHub* HubForTeam = (Team == 0) ? AttackerHub : DefenderHub;

	// 懒查找（AGameModeBase 不跑 BeginPlay，依赖 BeginPlay 找的 Hub 可能是 nullptr）
	if (!HubForTeam)
	{
		for (TActorIterator<ASpawnAreaHub> It(GetWorld()); It; ++It)
		{
			ASpawnAreaHub* Hub = *It;
			if (Hub->Team == Team)
			{
				HubForTeam = Hub;
				if (Team == 0) AttackerHub = Hub; else DefenderHub = Hub;
				break;
			}
		}
		// 兜底：找任何 SpawnHub
		if (!HubForTeam)
		{
			for (TActorIterator<ASpawnAreaHub> It(GetWorld()); It; ++It)
			{
				HubForTeam = *It;
				if (Team == 0) AttackerHub = HubForTeam; else DefenderHub = HubForTeam;
				break;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("SpawnDefaultPawnFor team=%d: 懒查找 Hub=%s"),
			Team, HubForTeam ? *HubForTeam->GetName() : TEXT("None"));
	}

	FVector SpawnLocation = StartSpot ? StartSpot->GetActorLocation() : FVector::ZeroVector;
	FRotator SpawnRotation = StartSpot ? StartSpot->GetActorRotation() : FRotator::ZeroRotator;

	if (HubForTeam)
	{
		if (AActor* SpawnPoint = HubForTeam->GetRandomSpawnPoint())
		{
			SpawnLocation = SpawnPoint->GetActorLocation();
			SpawnRotation = SpawnPoint->GetActorRotation();
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = NewPlayer;

	ABreakthroughCharacter* NewPawn = GetWorld()->SpawnActor<ABreakthroughCharacter>(PawnClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (NewPawn)
	{
		NewPawn->Team = Team;
		UE_LOG(LogTemp, Warning, TEXT("SpawnDefaultPawnFor: spawned team=%d (attacker) at %s"), Team, *SpawnLocation.ToString());
	}

	return NewPawn;
}

void AGameMode_Breakthrough::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// V1 single-player simulation: spawn 1 defender AI character at the defender spawn hub
	// (NOT inside the anchor) so attackers walking into the anchor zone get attacker > defender
	// headcount and can capture. Place it at the defender hub + a small Z offset so it drops onto Floor.
	if (bDefenderAISpawned)
	{
		return;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	if (DefenderHub)
	{
		SpawnLocation = DefenderHub->GetActorLocation() + FVector(0.f, 0.f, 100.f);
	}
	else
	{
		// 兜底：找 Team=1 的 SpawnAreaHub
		for (TActorIterator<ASpawnAreaHub> It(GetWorld()); It; ++It)
		{
			if (It->Team == 1)
			{
				SpawnLocation = It->GetActorLocation() + FVector(0.f, 0.f, 100.f);
				break;
			}
		}
		if (SpawnLocation.IsNearlyZero())
		{
			SpawnLocation = FVector(400.f, 0.f, 100.f);
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABreakthroughCharacter* DefenderAI = GetWorld()->SpawnActor<ABreakthroughCharacter>(
		ABreakthroughCharacter::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params);
	if (DefenderAI)
	{
		DefenderAI->Team = 1;
		bDefenderAISpawned = true;
		UE_LOG(LogTemp, Warning, TEXT("PostLogin: spawned defender AI at %s"), *SpawnLocation.ToString());
	}
}
