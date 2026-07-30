// MidanTelemetry — capture, serialisation, ghost replay.
//
// Depends on MidanCore only. Per-vehicle state is pulled through
// IMidanTelemetrySource, so the serialisation round-trip spec runs without
// spawning a vehicle. See docs/ASSUMPTIONS.md A8.

using UnrealBuildTool;

public class MidanTelemetry : ModuleRules
{
	public MidanTelemetry(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"MidanCore"
		});
	}
}
