// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MCPConfigureMapCommandlet.generated.h"

// Headless editor utility for deterministic map-level settings. It exists so the
// MCP development workflow can configure WorldSettings without depending on a
// visible editor viewport. Usage:
// UE4Editor-Cmd <project> -run=MCPConfigureMap -Map=/Game/Maps/NewMap
//   -GameModeClass=/Game/Blueprints/BP_GameMode_Conquest.BP_GameMode_Conquest_C
UCLASS()
class MCPGAMEPROJECT_API UMCPConfigureMapCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMCPConfigureMapCommandlet();
	virtual int32 Main(const FString& Params) override;
};
