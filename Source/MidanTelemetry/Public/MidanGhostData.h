// Recorded inputs + periodic state resync keys.
//
// Responsibility: the data a ghost replay is made of.
// Single reason to change: what a ghost recording stores changes.
//
// Input replay, not transform replay, is the point: master prompt §6.2 and
// docs/ARCHITECTURE.md §3.5 both say input replay demonstrates deterministic
// physics, and transform replay is a cheap trick that only LOOKS like the
// same thing. FMidanGhostResyncKey exists purely as a drift-correction
// safety net for the (expected, physics is not bit-exact across runs)
// divergence input replay alone would otherwise accumulate.

#pragma once

#include "CoreMinimal.h"
#include "MidanCoreTypes.h"

/** One periodic full-state snapshot, used to detect and correct drift. */
struct FMidanGhostResyncKey
{
	float TimeSeconds = 0.f;
	FTransform Transform = FTransform::Identity;
	FVector LinearVelocity = FVector::ZeroVector;

	friend FArchive& operator<<(FArchive& Ar, FMidanGhostResyncKey& Key)
	{
		Ar << Key.TimeSeconds;
		Ar << Key.Transform;
		Ar << Key.LinearVelocity;
		return Ar;
	}
};

/** One recorded input sample. Separate from FMidanTelemetryFrame (which also
 *  carries input) because a ghost recording is optimised for REPLAY —
 *  sequential access, input-only in the hot path — while a telemetry frame
 *  is optimised for ANALYSIS — every field, random access from Python. */
struct FMidanGhostInputSample
{
	float TimeSeconds = 0.f;
	FMidanVehicleInputState Input;

	friend FArchive& operator<<(FArchive& Ar, FMidanGhostInputSample& Sample)
	{
		Ar << Sample.TimeSeconds;
		Ar << Sample.Input.Throttle;
		Ar << Sample.Input.Brake;
		Ar << Sample.Input.Steer;
		Ar << Sample.Input.Handbrake;
		Ar << Sample.Input.bShiftUp;
		Ar << Sample.Input.bShiftDown;
		return Ar;
	}
};

struct MIDANTELEMETRY_API FMidanGhostRecording
{
	/** Format version — same reasoning as MidanTelemetryConstants::CurrentFormatVersion. */
	static constexpr int32 CurrentFormatVersion = 1;

	TArray<FMidanGhostInputSample> InputFrames;
	TArray<FMidanGhostResyncKey> ResyncKeys;

	/** Vehicle setup asset name this recording was captured against — a
	 *  ghost recorded on one car replayed on another would produce nonsense
	 *  (different mass, different torque curve), so the player is warned
	 *  rather than silently handed a car that drives wrong. */
	FString VehicleAssetName;

	friend FArchive& operator<<(FArchive& Ar, FMidanGhostRecording& Recording)
	{
		Ar << Recording.InputFrames;
		Ar << Recording.ResyncKeys;
		Ar << Recording.VehicleAssetName;
		return Ar;
	}
};

namespace MidanGhostIO
{
	/** Same versioned-magic-header pattern as MidanTelemetryWriter — see
	 *  that file's comment for why the format is self-checking. */
	MIDANTELEMETRY_API bool SaveRecording(const FString& FilePath, const FMidanGhostRecording& Recording);
	MIDANTELEMETRY_API bool LoadRecording(const FString& FilePath, FMidanGhostRecording& OutRecording);
}
