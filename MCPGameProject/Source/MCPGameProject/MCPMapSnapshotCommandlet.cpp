// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPMapSnapshotCommandlet.h"

// Snapshotting uses UnrealEd ObjectTools and is intentionally editor-only. Shipping
// and Development game targets compile the small stub at the end of this file.
#if WITH_EDITOR
#include "Editor.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

UMCPMapSnapshotCommandlet::UMCPMapSnapshotCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UMCPMapSnapshotCommandlet::Main(const FString& Params)
{
	FString SourcePath;
	FString DestinationPath;
	FParse::Value(*Params, TEXT("Source="), SourcePath);
	FParse::Value(*Params, TEXT("Destination="), DestinationPath);
	if (SourcePath.IsEmpty() || DestinationPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Usage: -Source=/Game/Maps/MainMap -Destination=/Game/Maps/BackUpMap"));
		return 1;
	}

	const FString SourceFilename = FPackageName::LongPackageNameToFilename(SourcePath, FPackageName::GetMapPackageExtension());
	const FString DestinationFilename = FPackageName::LongPackageNameToFilename(DestinationPath, FPackageName::GetMapPackageExtension());
	if (FPackageName::DoesPackageExist(DestinationPath))
	{
		UE_LOG(LogTemp, Error, TEXT("MCPMapSnapshot: destination already exists: %s"), *DestinationPath);
		return 2;
	}

	UPackage* SourcePackage = LoadPackage(nullptr, *SourceFilename, LOAD_None);
	UWorld* SourceWorld = SourcePackage
		? FindObject<UWorld>(SourcePackage, *FPackageName::GetLongPackageAssetName(SourcePath))
		: nullptr;
	if (!SourceWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("MCPMapSnapshot: failed to load source world %s"), *SourcePath);
		return 3;
	}

	ObjectTools::FPackageGroupName Target;
	Target.PackageName = DestinationPath;
	Target.ObjectName = FPackageName::GetLongPackageAssetName(DestinationPath);
	TSet<UPackage*> RefusedPackages;
	UWorld* SnapshotWorld = Cast<UWorld>(ObjectTools::DuplicateSingleObject(SourceWorld, Target, RefusedPackages, false));
	if (!SnapshotWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("MCPMapSnapshot: duplicate failed %s -> %s"), *SourcePath, *DestinationPath);
		return 4;
	}

	UPackage* SnapshotPackage = SnapshotWorld->GetOutermost();
	SnapshotPackage->MarkAsFullyLoaded();
	const bool bSaved = UPackage::SavePackage(SnapshotPackage, SnapshotWorld, RF_Public | RF_Standalone,
		*DestinationFilename, GError, nullptr, false, true, SAVE_NoError);
	if (!bSaved)
	{
		UE_LOG(LogTemp, Error, TEXT("MCPMapSnapshot: save failed %s"), *DestinationPath);
		return 5;
	}

	UE_LOG(LogTemp, Display, TEXT("MCPMapSnapshot: saved %s -> %s"), *SourcePath, *DestinationPath);
	return 0;
}
#else
UMCPMapSnapshotCommandlet::UMCPMapSnapshotCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UMCPMapSnapshotCommandlet::Main(const FString& Params)
{
	UE_LOG(LogTemp, Error, TEXT("MCPMapSnapshotCommandlet is editor-only and unavailable in a packaged game."));
	return 1;
}
#endif // WITH_EDITOR
