// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MCPConfigureMapCommandlet.generated.h"

// Headless editor utility for deterministic map-level settings and map snapshots.
// Usage:
// UE4Editor-Cmd <project> -run=MCPConfigureMap -SnapshotSource=/Game/Maps/SOLNewMap
//   -SnapshotDestination=/Game/Maps/SOLBackUpMap
// UE4Editor-Cmd <project> -run=MCPConfigureMap -Map=/Game/Maps/SOLNewMap
//   -GameModeClass=/Game/Blueprints/BP_X.BP_X_C
UCLASS()
class SOL_API UMCPConfigureMapCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMCPConfigureMapCommandlet();
	virtual int32 Main(const FString& Params) override;
};
