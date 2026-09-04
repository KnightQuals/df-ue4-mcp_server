// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SOL : ModuleRules
{
	public SOL(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// AIModule: the scavenger NPC is possessed by a plain AAIController so
		// that AddMovementInput works (a Character with no Controller silently
		// refuses to move). No behaviour tree / NavMesh is used — the module is
		// needed purely for the controller class.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore", "AIModule" });

		// Commandlets that edit/save a .umap are editor-only; keep UnrealEd out of
		// non-editor targets so packaged builds stay clean.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
