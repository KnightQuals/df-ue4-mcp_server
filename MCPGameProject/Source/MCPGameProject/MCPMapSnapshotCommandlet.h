// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MCPMapSnapshotCommandlet.generated.h"

// Creates an independent UWorld package copy for the project's NewMap -> BackUpMap
// workflow, without raw file copying or a visible editor session.
UCLASS()
class MCPGAMEPROJECT_API UMCPMapSnapshotCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMCPMapSnapshotCommandlet();
	virtual int32 Main(const FString& Params) override;
};
