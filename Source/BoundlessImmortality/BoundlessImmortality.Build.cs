// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BoundlessImmortality : ModuleRules
{
	public BoundlessImmortality(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Translation units reuse file-local helper names (Number, Obj, Flag...); unity blobs make them collide.
		bUseUnity = false;
		PrivateIncludePaths.Add(ModuleDirectory);
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Niagara", "LiveInteraction", "UMG" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities", "AssetRegistry", "HTTP", "NavigationSystem", "RenderCore", "RHI", "Slate", "SlateCore", "NiagaraUIRenderer" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
