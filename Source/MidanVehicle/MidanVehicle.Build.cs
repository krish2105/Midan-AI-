// MidanVehicle — vehicle pawn, physics configuration, feel systems.
//
// Depends on MidanCore only, among project modules.
// Feature dependencies land at the phase that needs them: EnhancedInput at
// Phase 3, Niagara and AudioMixer at Phase 4.

using UnrealBuildTool;

public class MidanVehicle : ModuleRules
{
	public MidanVehicle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"PhysicsCore",
			"Chaos",
			"ChaosVehicles",
			"MidanCore"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"InputCore",
			// Phase 3: UVehicleInputComponent binds Enhanced Input actions and
			// pushes the driving mapping context. Private because no public
			// header exposes an Enhanced Input type — the input config asset
			// holds only TSoftObjectPtr, which needs a forward declaration.
			"EnhancedInput"
		});
	}
}
