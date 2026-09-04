// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

// Dedicated Server build target (DS architecture, decided 2026-08-28):
// a headless server process with no rendering stack. The same SOL module
// source compiles into both the server and the client executables; runtime
// boundaries are drawn with HasAuthority() / IsRunningDedicatedServer()
// checks and the replication set declared in the actor classes.
public class SOLProjectServerTarget : TargetRules
{
	public SOLProjectServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		ExtraModuleNames.AddRange(new string[] { "SOL" });
	}
}
