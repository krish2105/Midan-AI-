// Packed capture record: one sample, one vehicle, one instant.
//
// Responsibility: the on-disk and in-ring-buffer shape of one telemetry
// sample.
// Single reason to change: a new field needs capturing.
//
// Deliberately a PLAIN struct, not a USTRUCT — this is written into a
// preallocated ring buffer at 60Hz and serialised as raw bytes by
// FMidanTelemetryBinaryWriter, and neither path benefits from UHT reflection
// or UPROPERTY overhead. Every member is a POD type (or FName/FVector/FQuat,
// all of which support `operator<<(FArchive&)` natively), so the whole
// struct serialises with one function.
//
// A flattened combination of FMidanVehicleFrameState, FMidanVehicleInputState
// and FMidanTrackPosition (all MidanCore) rather than embedding those structs
// directly — the capture record is a stable ON-DISK FORMAT, and coupling it
// to structs that change for physics or race-flow reasons would mean an old
// capture file's meaning silently drifts every time either of those changes.
// BuildFromSource is the one place that mapping happens.

#pragma once

#include "CoreMinimal.h"
#include "MidanCoreTypes.h"

namespace MidanTelemetryConstants
{
	static constexpr int32 NumWheels = MidanVehicleConstants::NumWheels;

	/** Binary format version. Bump when FMidanTelemetryFrame's serialised
	 *  shape changes, so a reader can refuse (or migrate) an old file
	 *  instead of silently misinterpreting its bytes. */
	static constexpr int32 CurrentFormatVersion = 1;
}

struct FMidanTelemetryWheelSample
{
	float SlipRatio = 0.f;
	float SlipAngleDegrees = 0.f;
	float NormalLoadN = 0.f;
	FName SurfaceTagName = NAME_None;
	bool bInContact = false;
	bool bOffTrack = false;

	friend FArchive& operator<<(FArchive& Ar, FMidanTelemetryWheelSample& Wheel)
	{
		Ar << Wheel.SlipRatio;
		Ar << Wheel.SlipAngleDegrees;
		Ar << Wheel.NormalLoadN;
		Ar << Wheel.SurfaceTagName;
		Ar << Wheel.bInContact;
		Ar << Wheel.bOffTrack;
		return Ar;
	}
};

struct MIDANTELEMETRY_API FMidanTelemetryFrame
{
	/** Seconds since capture began — matches FMidanVehicleFrameState's own
	 *  monotonic-clock contract, not world time. */
	double TimestampSeconds = 0.0;

	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FVector LinearVelocity = FVector::ZeroVector;

	float ForwardSpeedKmh = 0.f;
	float EngineRPM = 0.f;
	int32 Gear = 0;
	float LateralG = 0.f;
	float LongitudinalG = 0.f;
	float ChassisSlipAngleDegrees = 0.f;

	FMidanTelemetryWheelSample Wheels[MidanTelemetryConstants::NumWheels];

	float Throttle = 0.f;
	float Brake = 0.f;
	float Steer = 0.f;
	float Handbrake = 0.f;

	/** Arc length along the track centreline, cm — not the racing line;
	 *  telemetry analysis (deviation heat map) compares actual position
	 *  against the racing line separately, and the track's own arc length is
	 *  the stable axis every chart in Tools/analysis plots against. */
	float TrackArcLengthCm = 0.f;

	/** Signed offset from the track centreline, cm — positive right of
	 *  travel, same convention as FMidanTrackPosition::LateralOffset. The
	 *  input to Tools/analysis/telemetry_report.py's racing-line deviation
	 *  heat map. */
	float TrackLateralOffsetCm = 0.f;

	int32 LapIndex = 0;
	int32 SectorIndex = 0;

	/** Stable per-source identifier this sample came from —
	 *  IMidanTelemetrySource::GetTelemetrySourceId() — so a merged capture
	 *  file distinguishes the player from seven opponents. */
	FName SourceId = NAME_None;

	void BuildFromSource(
		const FMidanVehicleFrameState& VehicleState,
		const FMidanVehicleInputState& Input,
		double InTimestampSeconds,
		float InTrackArcLengthCm,
		float InTrackLateralOffsetCm,
		int32 InLapIndex,
		int32 InSectorIndex,
		FName InSourceId);

	friend FArchive& operator<<(FArchive& Ar, FMidanTelemetryFrame& Frame);
};
