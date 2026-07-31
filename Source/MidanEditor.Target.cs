// Editor target. The five runtime modules plus the editor-only tooling
// module and MidanTests (Phase 10) — an Editor target is never Shipping, so
// the functional-test module is always available to run in-editor.

using UnrealBuildTool;
using System.Collections.Generic;

public class MidanEditorTarget : TargetRules
{
	public MidanEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.AddRange(new[]
		{
			"MidanCore",
			"MidanVehicle",
			"MidanRace",
			"MidanAI",
			"MidanTelemetry",
			"MidanEditorTools",
			"MidanTests"
		});
	}
}
