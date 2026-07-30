// MidanCore — shared types, interfaces, gameplay tags, math.
//
// Depends on nothing project-side. This is load-bearing: every other module
// depends on Core, so a dependency added here propagates everywhere and a
// cycle becomes impossible to break. Do not add a Midan* module to this list.

using UnrealBuildTool;

public class MidanCore : ModuleRules
{
	public MidanCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public: types from these appear in MidanCore's public headers.
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"DeveloperSettings",
			"PhysicsCore"
		});
	}
}
