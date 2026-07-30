// Project configuration that is NOT tuning.
//
// Responsibility: project-wide switches and asset defaults.
// Single reason to change: a new project-level switch is needed.
//
// The line this class must not cross: nothing here affects how a car FEELS.
// Handling, camera, audio and AI values all live in Data Assets per CLAUDE.md.
// What belongs here is infrastructure — capture rates, debug visualisation
// toggles, and default asset references — the things a programmer sets once and
// a designer never touches.
//
// If you are tempted to add a float here that a human would tune by feel, it
// belongs in a Data Asset instead. That is the whole rule.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MidanDeveloperSettings.generated.h"

class UVehicleSetupDataAsset;
class USurfaceResponseDataAsset;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Midan"))
class MIDANCORE_API UMidanDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMidanDeveloperSettings();

	static const UMidanDeveloperSettings* Get();

	//~ Telemetry infrastructure --------------------------------------------

	/**
	 * Telemetry capture rate, Hz.
	 *
	 * Master prompt §6.1 fixes this at 60Hz decoupled from frame rate. Exposed
	 * because it is a capture-fidelity/file-size trade, not a feel value — and
	 * because the analysis tooling needs to know it to reconstruct a time axis.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Telemetry", meta = (ClampMin = "10", ClampMax = "240"))
	int32 TelemetryCaptureHz = 60;

	/**
	 * Preallocated ring buffer capacity, in samples per source.
	 *
	 * Sized once at startup and never grown — CLAUDE.md forbids allocation in
	 * the capture path. At 60Hz, 7200 samples is two minutes per vehicle, which
	 * comfortably covers a three-lap race plus the flush latency.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Telemetry", meta = (ClampMin = "600", ClampMax = "64000"))
	int32 TelemetryRingBufferSamples = 7200;

	/** Off by default. Capture is a development and profiling tool, and a
	 *  Shipping build should not pay for it unless deliberately enabled. */
	UPROPERTY(Config, EditAnywhere, Category = "Telemetry")
	bool bTelemetryCaptureEnabledByDefault = false;

	//~ Default assets ------------------------------------------------------

	/**
	 * Surface response table. Soft, and async-loaded.
	 *
	 * TSoftObjectPtr because CLAUDE.md forbids hard asset references —
	 * particularly here, since a UDeveloperSettings CDO is constructed at engine
	 * startup and a hard reference would drag the asset and its whole
	 * dependency chain into memory before the first map loads.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Defaults", meta = (AllowedClasses = "/Script/MidanVehicle.SurfaceResponseDataAsset"))
	TSoftObjectPtr<USurfaceResponseDataAsset> DefaultSurfaceResponse;

	/** Fallback vehicle setup, used when a pawn is spawned without one — which
	 *  happens constantly during Phase 3 testing in an empty level. */
	UPROPERTY(Config, EditAnywhere, Category = "Defaults", meta = (AllowedClasses = "/Script/MidanVehicle.VehicleSetupDataAsset"))
	TSoftObjectPtr<UVehicleSetupDataAsset> FallbackVehicleSetup;

	//~ Debug visualisation -------------------------------------------------
	// All compiled out of Shipping. A debug draw that survives into a shipped
	// build is both a visual defect and a frame cost nobody budgeted for.

	/** Draw aero force vectors at their application points. The fastest way to
	 *  confirm front/rear downforce balance is behaving as authored. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bDebugDrawAeroForces = false;

	/** Draw per-wheel surface tag and contact state. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bDebugDrawWheelSurfaces = false;

	/** Log every assist intervention. Verbose by nature — traction control fires
	 *  many times per second — so it is off by default and log-only. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bDebugLogAssistInterventions = false;

	/**
	 * Warn when a physics callback exceeds this many microseconds.
	 *
	 * Not a tuning value: it is a tripwire for the CLAUDE.md rule that the
	 * callback stays cheap and allocation-free. 250us at 120Hz is 3% of a frame,
	 * which is already more than the callback should ever need.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Debug", meta = (ClampMin = "50", ClampMax = "5000"))
	int32 PhysicsCallbackWarnThresholdMicroseconds = 250;

	/** True only when the corresponding toggle is set AND this is not Shipping. */
	bool IsAeroDebugDrawEnabled() const;
	bool IsWheelSurfaceDebugDrawEnabled() const;

	/** Fixed timestep the telemetry accumulator advances by. */
	double GetTelemetryTimestepSeconds() const
	{
		return 1.0 / static_cast<double>(FMath::Max(1, TelemetryCaptureHz));
	}
};
