// MidanEditorTools — validators, spline tools, commandlets. EDITOR ONLY.
//
// Listed in MidanEditor.Target.cs and absent from Midan.Target.cs. That
// absence, not a #if WITH_EDITOR, is what keeps these symbols out of a
// Shipping binary — they are never compiled into that target at all.
//
// All dependencies are PRIVATE. Nothing outside this module may include its
// headers, and no runtime module may ever depend on it.
//
// Keep this module thin: the algorithms live in runtime modules where an
// Automation Spec can reach them. Each class here is a Python-callable
// wrapper. If a wrapper grows logic worth testing, the split is wrong.

using UnrealBuildTool;

public class MidanEditorTools : ModuleRules
{
	public MidanEditorTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Slate",
			"SlateCore",
			"Projects",
			"AssetRegistry",
			"EditorScriptingUtilities",
			"DataValidation",
			"GameplayTags",
			"MidanCore",
			"MidanVehicle",
			"MidanRace",
			"MidanAI",
			"MidanTelemetry",

			// WorldPartitionHLODsBuilder (MidanBuildHLODCommandlet, Phase 8)
			// — module name unconfirmed without an installed 5.8 engine; it
			// has moved between "WorldPartitionEditor" and a dedicated HLOD
			// utilities module across UE5 releases. See
			// docs/ASSUMPTIONS.md and MidanBuildHLODCommandlet.cpp's API
			// VERIFY comment.
			"WorldPartitionEditor",
			"WorldPartitionHLODUtilities"
		});
	}
}
