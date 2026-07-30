// Native gameplay tag vocabulary for the whole project.
//
// Pulled forward from Phase 3 to Phase 2 (see docs/ASSUMPTIONS.md A20):
// UVehicleSetupDataAsset::ValidateData must check that VehicleClass descends
// from Vehicle.Class, and CLAUDE.md forbids constructing a tag from a string
// literal at a call site — so the native tag had to exist before the
// validator that uses it could be written correctly.
//
// Declare every tag once, here. Never call
// FGameplayTag::RequestGameplayTag(FName(TEXT("..."))) at a call site.

#pragma once

#include "NativeGameplayTags.h"

namespace MidanTags
{
	// --- Vehicle class --------------------------------------------------
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Vehicle_Class);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Vehicle_Class_Hyper);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Vehicle_Class_GT);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Vehicle_Class_Rally);

	// --- Vehicle camera modes (Phase 4) ----------------------------------
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Vehicle_Camera_ChaseFar);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Vehicle_Camera_ChaseNear);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Vehicle_Camera_Bonnet);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Vehicle_Camera_Cockpit);

	// --- Vehicle assists (Phase 3) ---------------------------------------
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Vehicle_Assist_TractionControl);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Vehicle_Assist_ABS);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Vehicle_Assist_Stability);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Vehicle_Assist_SteeringAssist);

	// --- Race state machine (Phase 5) ------------------------------------
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Race_State_Grid);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Race_State_Countdown);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Race_State_Racing);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Race_State_Finished);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Race_State_Results);

	// --- Surfaces (Phase 3 sensor, Phase 5 off-track detection) ----------
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Surface_Tarmac);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Surface_Kerb);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Surface_Gravel);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Surface_Grass);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Surface_Sand);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Surface_Wet);

	// --- Telemetry events (Phase 6, Phase 9) -----------------------------
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Telemetry_Event_RubberBand);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Telemetry_Event_Mistake);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Telemetry_Event_OffTrack);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Telemetry_Event_LapInvalidated);
	MIDANCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Telemetry_Event_Respawn);
}
