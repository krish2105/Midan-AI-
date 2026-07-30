// Game target. Ships the five runtime modules.
//
// MidanEditorTools is deliberately absent: its absence from this list is what
// guarantees no editor-only symbol can reach a Shipping binary, and
// Tools/build/verify_build.py treats a violation as a hard gate failure.

using UnrealBuildTool;
using System.Collections.Generic;

public class MidanTarget : TargetRules
{
	public MidanTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.AddRange(new[]
		{
			"MidanCore",
			"MidanVehicle",
			"MidanRace",
			"MidanAI",
			"MidanTelemetry"
		});
	}
}
