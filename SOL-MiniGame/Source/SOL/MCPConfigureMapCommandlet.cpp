// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPConfigureMapCommandlet.h"

// This commandlet mutates editor map packages and references UnrealEd's ObjectTools.
// Keep those dependencies out of the packaged game target; game builds provide a
// harmless stub implementation below.
#if WITH_EDITOR
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

UMCPConfigureMapCommandlet::UMCPConfigureMapCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UMCPConfigureMapCommandlet::Main(const FString& Params)
{
	UE_LOG(LogTemp, Display, TEXT("MCPConfigureMap: Main started, params: %s"), *Params);
	FString MapPath;
	FString GameModeClassPath;
	FString SnapshotSource;
	FString SnapshotDestination;
	FParse::Value(*Params, TEXT("Map="), MapPath);
	FParse::Value(*Params, TEXT("GameModeClass="), GameModeClassPath);
	FParse::Value(*Params, TEXT("SnapshotSource="), SnapshotSource);
	FParse::Value(*Params, TEXT("SnapshotDestination="), SnapshotDestination);

	// Snapshot mode: make an independent package copy with UE's ObjectTools route.
	// Raw .umap file copies are forbidden (duplicate PrimaryAssetID alarms).
	if (!SnapshotSource.IsEmpty() || !SnapshotDestination.IsEmpty())
	{
		if (SnapshotSource.IsEmpty() || SnapshotDestination.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("Snapshot mode needs both -SnapshotSource and -SnapshotDestination"));
			return 1;
		}
		if (FPackageName::DoesPackageExist(SnapshotDestination))
		{
			UE_LOG(LogTemp, Error, TEXT("Snapshot destination already exists: %s"), *SnapshotDestination);
			return 2;
		}
		const FString SourceFilename = FPackageName::LongPackageNameToFilename(SnapshotSource, FPackageName::GetMapPackageExtension());
		UPackage* SourcePackage = LoadPackage(nullptr, *SourceFilename, LOAD_None);
		UWorld* SourceWorld = SourcePackage
			? FindObject<UWorld>(SourcePackage, *FPackageName::GetLongPackageAssetName(SnapshotSource))
			: nullptr;
		if (!SourceWorld)
		{
			UE_LOG(LogTemp, Error, TEXT("Snapshot source UWorld not found: %s"), *SnapshotSource);
			return 3;
		}
		ObjectTools::FPackageGroupName Target;
		Target.PackageName = SnapshotDestination;
		Target.ObjectName = FPackageName::GetLongPackageAssetName(SnapshotDestination);
		TSet<UPackage*> RefusedPackages;
		UWorld* SnapshotWorld = Cast<UWorld>(ObjectTools::DuplicateSingleObject(SourceWorld, Target, RefusedPackages, false));
		if (!SnapshotWorld)
		{
			UE_LOG(LogTemp, Error, TEXT("Snapshot duplication failed: %s -> %s"), *SnapshotSource, *SnapshotDestination);
			return 4;
		}
		UPackage* SnapshotPackage = SnapshotWorld->GetOutermost();
		SnapshotPackage->MarkAsFullyLoaded();
		const FString SnapshotFilename = FPackageName::LongPackageNameToFilename(SnapshotDestination, FPackageName::GetMapPackageExtension());
		const bool bSnapshotSaved = UPackage::SavePackage(SnapshotPackage, SnapshotWorld, RF_Public | RF_Standalone,
			*SnapshotFilename, GError, nullptr, false, true, SAVE_NoError);
		if (!bSnapshotSaved)
		{
			UE_LOG(LogTemp, Error, TEXT("Snapshot package save failed: %s"), *SnapshotDestination);
			return 5;
		}
		UE_LOG(LogTemp, Display, TEXT("MCPConfigureMap: snapshot saved %s -> %s"), *SnapshotSource, *SnapshotDestination);
		return 0;
	}

	if (MapPath.IsEmpty() || GameModeClassPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Usage: -Map=... -GameModeClass=...  or  -SnapshotSource=... -SnapshotDestination=..."));
		return 1;
	}

	// Load the map package directly rather than opening an editor world. This keeps
	// the commandlet fully headless and avoids viewport/map-switch instability.
	const FString MapFilename = FPackageName::LongPackageNameToFilename(MapPath, FPackageName::GetMapPackageExtension());
	UPackage* MapPackage = LoadPackage(nullptr, *MapFilename, LOAD_None);
	if (!MapPackage)
	{
		UE_LOG(LogTemp, Error, TEXT("MCPConfigureMap: failed to load map package %s"), *MapPath);
		return 2;
	}
	UWorld* World = FindObject<UWorld>(MapPackage, *FPackageName::GetLongPackageAssetName(MapPath));
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("MCPConfigureMap: no UWorld named %s inside package"), *MapPath);
		return 2;
	}

	UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, *GameModeClassPath);
	if (!GameModeClass)
	{
		UE_LOG(LogTemp, Error, TEXT("MCPConfigureMap: failed to load GameMode class %s"), *GameModeClassPath);
		return 3;
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!WorldSettings)
	{
		UE_LOG(LogTemp, Error, TEXT("MCPConfigureMap: map %s has no WorldSettings"), *MapPath);
		return 4;
	}

	WorldSettings->Modify();
	WorldSettings->DefaultGameMode = GameModeClass;
	WorldSettings->MarkPackageDirty();

	const bool bSaved = UPackage::SavePackage(MapPackage, World, RF_Public | RF_Standalone,
		*MapFilename, GError, nullptr, false, true, SAVE_NoError);
	if (!bSaved)
	{
		UE_LOG(LogTemp, Error, TEXT("MCPConfigureMap: set GameMode but failed to save %s"), *MapPath);
		return 5;
	}

	UE_LOG(LogTemp, Display, TEXT("MCPConfigureMap: saved %s with GameMode %s"), *MapPath, *GameModeClassPath);
	return 0;
}
#else
UMCPConfigureMapCommandlet::UMCPConfigureMapCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UMCPConfigureMapCommandlet::Main(const FString& Params)
{
	UE_LOG(LogTemp, Error, TEXT("MCPConfigureMapCommandlet is editor-only and unavailable in a packaged game."));
	return 1;
}
#endif // WITH_EDITOR
