// Editor target. The five runtime modules plus the editor-only tooling module.

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
			"MidanEditorTools"
		});
	}
}
