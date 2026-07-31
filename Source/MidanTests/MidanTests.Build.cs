// MidanTests — functional (map-based) tests: vehicle spawn, AI lap
// completion, respawn, race completion.
//
// Distinct from the Automation Specs scattered through every other module's
// Private/Tests/ folder: those are pure-logic unit tests requiring no map
// (docs/ARCHITECTURE.md §5). These four need a running world, a spawned
// vehicle, and (for the AI and race tests) a populated grid — that is what
// AFunctionalTest and a dedicated test map are for.
//
// NEVER linked into a Shipping target — see Source/Midan.Target.cs and
// Source/MidanEditor.Target.cs, which both add this module conditionally on
// Target.Configuration != Shipping. Tools/build/verify_build.py's
// "no editor-only module referenced" hard gate extends to this module by the
// same reasoning MidanEditorTools already established: test code has no
// business in a shipped binary.

using UnrealBuildTool;

public class MidanTests : ModuleRules
{
	public MidanTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"FunctionalTesting",
			"GameplayTags",
			"MidanCore",
			"MidanVehicle",
			"MidanRace",
			"MidanAI"
		});
	}
}
