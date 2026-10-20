// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PolicePlatform : ModuleRules
{
	public PolicePlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "UMG" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
