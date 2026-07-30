// MidanRace — track, checkpoints, lap timing, race flow.
//
// Depends on MidanCore only. It deliberately does NOT depend on MidanVehicle:
// vehicle state is read through IMidanVehicleInterface, resolved via
// UMidanServiceLocatorSubsystem. That keeps lap validation and position
// calculation testable without spawning a vehicle. See docs/ASSUMPTIONS.md A7.
//
// UMG/Slate land at Phase 7 when the HUD arrives.

using UnrealBuildTool;

public class MidanRace : ModuleRules
{
	public MidanRace(ReadOnlyTargetRules Target) : base(Target)
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

		// AIModule supplies AAIController only — an engine module, so this
		// does not touch the project module graph in docs/ARCHITECTURE.md
		// §2.1. AMidanRaceGameMode possesses opponents with a plain
		// AAIController until AMidanOpponentController exists at Phase 6; see
		// docs/ASSUMPTIONS.md.
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AIModule"
		});
	}
}
