#include "MidanGameplayTags.h"

namespace MidanTags
{
	// Vehicle.Class is declared as a tag in its own right, not just a prefix,
	// so ValidateData can ask "does this descend from Vehicle.Class?" via
	// MatchesTag rather than doing string prefix comparison.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_Class, "Vehicle.Class", "Parent tag for vehicle classes. Not assignable directly.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_Class_Hyper, "Vehicle.Class.Hyper", "AWD hypercar. Brutal straight-line speed, high downforce, nervous below 80km/h.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_Class_GT, "Vehicle.Class.GT", "RWD GT. Progressive oversteer, holdable slides, forgiving at the limit.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_Class_Rally, "Vehicle.Class.Rally", "AWD rally. Slow on tarmac, unstoppable on gravel. High COM by design.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_Camera_ChaseFar, "Vehicle.Camera.ChaseFar", "Default chase camera, long spring arm.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_Camera_ChaseNear, "Vehicle.Camera.ChaseNear", "Close chase camera.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_Camera_Bonnet, "Vehicle.Camera.Bonnet", "Bonnet-mounted camera.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_Camera_Cockpit, "Vehicle.Camera.Cockpit", "Cockpit camera. Low interior detail in this slice.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_Assist_TractionControl, "Vehicle.Assist.TractionControl", "Limits drive torque on excessive wheel slip ratio.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_Assist_ABS, "Vehicle.Assist.ABS", "Modulates brake torque to keep slip ratio below lockup.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_Assist_Stability, "Vehicle.Assist.Stability", "Applies corrective yaw moment on excessive chassis slip angle.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_Assist_SteeringAssist, "Vehicle.Assist.SteeringAssist", "Biases steer input toward the recovery direction during a slide.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Race_State_Grid, "Race.State.Grid", "Cars placed on the grid, engines running, input locked.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Race_State_Countdown, "Race.State.Countdown", "Countdown running. Input still locked.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Race_State_Racing, "Race.State.Racing", "Green. Lap timing active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Race_State_Finished, "Race.State.Finished", "Leader has finished; remaining cars complete their lap.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Race_State_Results, "Race.State.Results", "Results screen. Telemetry flushed.");

	// Surface tags mirror the SurfaceTypeN entries in Config/DefaultEngine.ini.
	// The mapping between the two lives in USurfaceResponseDataAsset, which is
	// validated so a surface type cannot be silently unmapped.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_Tarmac, "Surface.Tarmac", "SurfaceType1. Baseline grip; friction multiplier 1.0 by convention.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_Kerb, "Surface.Kerb", "SurfaceType2. On-track. Does not invalidate a lap.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_Gravel, "Surface.Gravel", "SurfaceType3. Off-track. The surface the rally car exists for.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_Grass, "Surface.Grass", "SurfaceType4. Off-track.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_Sand, "Surface.Sand", "SurfaceType5. Off-track.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_Wet, "Surface.Wet", "SurfaceType6. On-track, reduced grip. Not dynamic weather.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Telemetry_Event_RubberBand, "Telemetry.Event.RubberBand", "Every rubber-band application, so its subtlety is provable.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Telemetry_Event_Mistake, "Telemetry.Event.Mistake", "AI deliberate mistake fired.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Telemetry_Event_OffTrack, "Telemetry.Event.OffTrack", "Off-track grace timer started.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Telemetry_Event_LapInvalidated, "Telemetry.Event.LapInvalidated", "Lap rejected by the timing subsystem.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Telemetry_Event_Respawn, "Telemetry.Event.Respawn", "Respawn to last valid checkpoint.");
}
