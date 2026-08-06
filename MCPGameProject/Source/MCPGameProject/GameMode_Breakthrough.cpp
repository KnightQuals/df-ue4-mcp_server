// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameMode_Breakthrough.h"
#include "SpawnAreaHub.h"
#include "BreakthroughCharacter.h"
#include "BattleSectorAnchor.h"
#include "DefaultPlayerController.h"
#include "ForbiddenZone.h"
#include "BattleAreaSpline.h"
#include "BattleSectorBase.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/World.h"

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

	// V2: apply DataTable-driven match / forbidden-zone settings, then build the
	// four physical outer-border triggers. This happens only on the authority GameMode.
	LoadAndApplyMapConfig();
	SpawnForbiddenZoneBorders();
	SpawnSafeBattlefieldSpline();

	UE_LOG(LogTemp, Warning, TEXT("GameMode_Breakthrough started, AttackerHub=%s DefenderHub=%s MapId=%s"),
		AttackerHub ? *AttackerHub->GetName() : TEXT("None"),
		DefenderHub ? *DefenderHub->GetName() : TEXT("None"), *MapId.ToString());
}

void AGameMode_Breakthrough::LoadAndApplyMapConfig()
{
	// The project ships a DataTable row for NewMap. Keep a safe C++ fallback so a
	// missing user-created asset never prevents the MiniGame from starting.
	if (!MapConfigTable)
	{
		MapConfigTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Config/DT_BattleMapConfig.DT_BattleMapConfig"));
	}

	if (MapConfigTable)
	{
		if (const FBattleMapConfig* Row = MapConfigTable->FindRow<FBattleMapConfig>(MapId, TEXT("GameMode_Breakthrough")))
		{
			ActiveMapConfig = *Row;
			UE_LOG(LogTemp, Warning, TEXT("V2 MapConfig loaded: MapId=%s Match=%.0fs Forbidden=%.0fs Safe=(%.0f, %.0f)"),
				*MapId.ToString(), ActiveMapConfig.MatchDuration, ActiveMapConfig.ForbiddenCountdown,
				ActiveMapConfig.SafeHalfExtentX, ActiveMapConfig.SafeHalfExtentY);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("V2 MapConfig row '%s' missing; using C++ defaults"), *MapId.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("V2 MapConfig DataTable missing; using C++ defaults"));
	}

	// Apply map-tuned values to existing gameplay actors.
	for (TActorIterator<ABattleSectorBase> It(GetWorld()); It; ++It)
	{
		It->MatchDuration = ActiveMapConfig.MatchDuration;
	}
	for (TActorIterator<ABattleSectorAnchor> It(GetWorld()); It; ++It)
	{
		It->CaptureSpeed = ActiveMapConfig.CaptureSpeed;
	}
}

void AGameMode_Breakthrough::SpawnForbiddenZoneBorders()
{
	if (!HasAuthority() || ForbiddenZones.Num() > 0)
	{
		return;
	}

	const float HalfX = ActiveMapConfig.SafeHalfExtentX;
	const float HalfY = ActiveMapConfig.SafeHalfExtentY;
	const float Border = ActiveMapConfig.ForbiddenBorderWidth;
	const float HalfBorder = Border * 0.5f;

	struct FBorderDefinition
	{
		FVector Location;
		FVector Extent;
		FString Label;
	};

	// Four strips deliberately overlap at their corners. Character keeps an overlap
	// count, so leaving one corner strip does not prematurely clear the countdown.
	const TArray<FBorderDefinition> Borders =
	{
		{ FVector(0.f, HalfY + HalfBorder, 80.f), FVector(HalfX + Border, HalfBorder, 350.f), TEXT("FORBIDDEN ZONE - RETURN TO BATTLEFIELD") },
		{ FVector(0.f, -HalfY - HalfBorder, 80.f), FVector(HalfX + Border, HalfBorder, 350.f), TEXT("FORBIDDEN ZONE - RETURN TO BATTLEFIELD") },
		{ FVector(HalfX + HalfBorder, 0.f, 80.f), FVector(HalfBorder, HalfY, 350.f), TEXT("FORBIDDEN ZONE - RETURN TO BATTLEFIELD") },
		{ FVector(-HalfX - HalfBorder, 0.f, 80.f), FVector(HalfBorder, HalfY, 350.f), TEXT("FORBIDDEN ZONE - RETURN TO BATTLEFIELD") },
	};

	for (const FBorderDefinition& BorderDef : Borders)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AForbiddenZone* Zone = GetWorld()->SpawnActor<AForbiddenZone>(AForbiddenZone::StaticClass(), BorderDef.Location, FRotator::ZeroRotator, Params);
		if (Zone)
		{
			Zone->ConfigureZone(BorderDef.Label, BorderDef.Extent);
			ForbiddenZones.Add(Zone);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("V2 ForbiddenZone: spawned %d outer border strips"), ForbiddenZones.Num());
}

void AGameMode_Breakthrough::SpawnSafeBattlefieldSpline()
{
	if (!HasAuthority() || SafeBattlefieldSpline)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SafeBattlefieldSpline = GetWorld()->SpawnActor<ABattleAreaSpline>(ABattleAreaSpline::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (SafeBattlefieldSpline)
	{
		SafeBattlefieldSpline->ConfigureRectangle(ActiveMapConfig.SafeHalfExtentX, ActiveMapConfig.SafeHalfExtentY);
		UE_LOG(LogTemp, Warning, TEXT("V2 Spline: spawned editable safe boundary (%.0f x %.0f)"),
			ActiveMapConfig.SafeHalfExtentX * 2.f, ActiveMapConfig.SafeHalfExtentY * 2.f);
	}
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
		NewPawn->ConfigureForbiddenZone(ActiveMapConfig.ForbiddenCountdown, ActiveMapConfig.RespawnDelay, ActiveMapConfig.SafeHalfExtentX, ActiveMapConfig.SafeHalfExtentY);
		UE_LOG(LogTemp, Warning, TEXT("SpawnDefaultPawnFor: spawned team=%d at %s (V2 forbidden=%.0fs)"),
			Team, *SpawnLocation.ToString(), ActiveMapConfig.ForbiddenCountdown);
	}

	return NewPawn;
}

void AGameMode_Breakthrough::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Defer the AI-defender decision by 1.5s: in 2-player PIE both humans join within
	// a fraction of a second, so after a short delay GetNumPlayers() tells us reliably
	// whether this is single-player (spawn AI defender) or multiplayer (skip AI, the
	// 2nd human is the defender). Only arm the timer once.
	if (bDefenderAISpawned || bAISpawnTimerArmed)
	{
		return;
	}
	bAISpawnTimerArmed = true;

	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AGameMode_Breakthrough::TrySpawnDefenderAI, 1.5f, false);
}

void AGameMode_Breakthrough::TrySpawnDefenderAI()
{
	if (bDefenderAISpawned)
	{
		return;
	}

	// If 2+ humans are connected, they fill both teams — no AI defender needed.
	const int32 NumPlayers = GetNumPlayers();
	if (NumPlayers >= 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("TrySpawnDefenderAI: %d players — multiplayer, skipping AI."), NumPlayers);
		bDefenderAISpawned = true;
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
			SpawnLocation = FVector(3000.f, 0.f, 100.f);
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABreakthroughCharacter* DefenderAI = GetWorld()->SpawnActor<ABreakthroughCharacter>(
		ABreakthroughCharacter::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params);
	if (DefenderAI)
	{
		DefenderAI->Team = 1;
		DefenderAI->ConfigureForbiddenZone(ActiveMapConfig.ForbiddenCountdown, ActiveMapConfig.RespawnDelay, ActiveMapConfig.SafeHalfExtentX, ActiveMapConfig.SafeHalfExtentY);
		DefenderAI->PatrolCenter = SpawnLocation;
		DefenderAI->PatrolRadius = 1200.f; // 24m box, inside the 30m SpawnHub area
		bDefenderAISpawned = true;
		UE_LOG(LogTemp, Warning, TEXT("TrySpawnDefenderAI: spawned defender AI at %s (single-player)"),
			*SpawnLocation.ToString());
	}
}
