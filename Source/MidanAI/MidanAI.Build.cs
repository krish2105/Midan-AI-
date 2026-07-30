// MidanAI — racing line, opponent controller.
//
// Depends on MidanCore + MidanVehicle. It does NOT depend on MidanRace or
// MidanTelemetry: race phase is read through IMidanRaceStateInterface and
// rubber-band events are pushed through IMidanTelemetrySink, both declared in
// MidanCore. See docs/ASSUMPTIONS.md A9 and A10.
//
// AIModule supplies AAIController, which AMidanOpponentController derives from.

using UnrealBuildTool;

public class MidanAI : ModuleRules
{
	public MidanAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"AIModule",
			"MidanCore",
			"MidanVehicle"
		});
	}
}
