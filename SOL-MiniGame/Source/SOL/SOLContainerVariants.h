// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SOLContainer.h"
#include "SOLContainerVariants.generated.h"

// Container variants. All of them are pure configuration on top of
// ASOLContainer — proportions, interaction radius, name — so replication,
// server-authoritative looting and the DataTable pool lookup are inherited
// wholesale. Adding a container type therefore costs no gameplay code, which
// is the whole point of keeping the base class generic.
//
// Loot pools live in DT_SOLContainers keyed by actor name, so a designer (or
// the MCP orchestrator) tunes what each one drops without touching C++.

// Military hardware crate: wide and flat, big interaction radius (it is a
// crate you stand next to, not a box you crouch over).
UCLASS()
class SOL_API ASOLArmoryCrate : public ASOLContainer
{
	GENERATED_BODY()
public:
	ASOLArmoryCrate();
};

// Server rack: tall and narrow, holds electronics. Small radius — you have to
// walk right up to it.
UCLASS()
class SOL_API ASOLServerRack : public ASOLContainer
{
	GENERATED_BODY()
public:
	ASOLServerRack();
};

// Smuggler's stash: small, awkward, high value density. Tightest radius of
// all so it is easy to walk straight past.
UCLASS()
class SOL_API ASOLContrabandCase : public ASOLContainer
{
	GENERATED_BODY()
public:
	ASOLContrabandCase();
};

// Field medical station: broad and low, generous radius (it is meant to be
// found in a hurry).
UCLASS()
class SOL_API ASOLMedStation : public ASOLContainer
{
	GENERATED_BODY()
public:
	ASOLMedStation();
};
