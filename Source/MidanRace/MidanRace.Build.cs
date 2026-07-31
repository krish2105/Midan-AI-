// MidanRace — track, checkpoints, lap timing, race flow.
//
// Depends on MidanCore only. It deliberately does NOT depend on MidanVehicle:
// vehicle state is read through IMidanVehicleInterface, resolved via
// UMidanServiceLocatorSubsystem. That keeps lap validation and position
// calculation testable without spawning a vehicle. See docs/ASSUMPTIONS.md A7.
//
// UMG/Slate/InputCore/EnhancedInput arrived at Phase 7 for the HUD — added
// only when Source/MidanRace/*/UI actually needed them, not speculatively.

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
		//
		// UMG/Slate/SlateCore back every UI/* widget and AMidanHUD.
		// InputCore/EnhancedInput back AMidanHUD's IA_Pause binding — the
		// same engine-module pattern as AIModule above, not a project-module
		// dependency.
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AIModule",
			"UMG",
			"Slate",
			"SlateCore",
			"InputCore",
			"EnhancedInput"
		});
	}
}
