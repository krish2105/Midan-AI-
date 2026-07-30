// The data contract between modules.
//
// Responsibility: plain-old-data types that cross a module boundary.
// Single reason to change: a system needs a field it cannot derive.
//
// Everything here is POD and allocation-free by construction. These structs are
// filled inside the async physics callback, where CLAUDE.md forbids allocation,
// so no member may be a TArray, FString, TMap, or anything else that heap
// allocates. Wheel data uses a fixed-size array for exactly this reason.
//
// These types live in MidanCore rather than MidanVehicle because MidanRace and
// MidanTelemetry consume them without depending on MidanVehicle — see
// docs/ASSUMPTIONS.md A7 and A8.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MidanCoreTypes.generated.h"

namespace MidanVehicleConstants
{
	/**
	 * Wheel count, fixed at four.
	 *
	 * A named constant rather than a Data Asset field because it is structural,
	 * not tuning: the suspension and tyre config structs are authored per-axle
	 * (front/rear), the surface sensor indexes a fixed array, and Chaos wheel
	 * setups are built one-per-wheel at apply time. Supporting a different count
	 * is a rewrite of all three, not a value change. Every vehicle in
	 * docs/VEHICLE_SPEC.md has four wheels.
	 */
	static constexpr int32 NumWheels = 4;

	/** Wheel indices. Order matches the Chaos wheel setup array built by
	 *  UVehicleSetupApplier, and the per-axle config structs it reads from. */
	static constexpr int32 WheelFrontLeft  = 0;
	static constexpr int32 WheelFrontRight = 1;
	static constexpr int32 WheelRearLeft   = 2;
	static constexpr int32 WheelRearRight  = 3;

	/** True for the two front wheels. Used by per-axle config lookup. */
	FORCEINLINE bool IsFrontWheel(const int32 WheelIndex)
	{
		return WheelIndex == WheelFrontLeft || WheelIndex == WheelFrontRight;
	}
}

/**
 * Normalised vehicle input. THE SINGLE INPUT CONTRACT.
 *
 * Both the player's UVehicleInputComponent and the AI's AMidanOpponentController
 * produce one of these and pass it to IMidanVehicleInterface::ApplyInput. There
 * is no second path into the vehicle, and that absence is the structural
 * guarantee that the AI cannot cheat physics — master prompt §4.2 makes this a
 * hard architectural rule, and a rule enforced by having only one door is worth
 * more than a rule enforced by review.
 *
 * All continuous values are already shaped. Steering here is the OUTPUT of the
 * steering curve, not raw stick deflection.
 */
USTRUCT(BlueprintType)
struct MIDANCORE_API FMidanVehicleInputState
{
	GENERATED_BODY()

	/** 0..1. Already curve-shaped. */
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	float Throttle = 0.f;

	/** 0..1. Already curve-shaped. */
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	float Brake = 0.f;

	/**
	 * -1..1, left negative. Already through the speed-sensitive steering curve
	 * and the rise/fall rate limiter. A consumer must never re-shape this.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	float Steer = 0.f;

	/** 0..1. Analogue so a partial pull is possible on a trigger. */
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	float Handbrake = 0.f;

	/** Edge-triggered, consumed once by the movement component. */
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	bool bShiftUp = false;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	bool bShiftDown = false;

	/** Clamps every channel into its legal range. Cheap insurance at the door:
	 *  an out-of-range input reaching the solver produces behaviour that looks
	 *  like a physics bug and is not one. */
	void Sanitise()
	{
		Throttle  = FMath::Clamp(Throttle, 0.f, 1.f);
		Brake     = FMath::Clamp(Brake, 0.f, 1.f);
		Handbrake = FMath::Clamp(Handbrake, 0.f, 1.f);
		Steer     = FMath::Clamp(Steer, -1.f, 1.f);
	}

	/** True when no control input is being applied. */
	bool IsNeutral() const
	{
		return Throttle < KINDA_SMALL_NUMBER
			&& Brake < KINDA_SMALL_NUMBER
			&& Handbrake < KINDA_SMALL_NUMBER
			&& FMath::IsNearlyZero(Steer);
	}
};

/**
 * Per-wheel physics state, sampled in the physics callback.
 *
 * Slip ratio and slip angle are the two numbers the assists, the FX layer and
 * the telemetry all key off. Surface comes from the physical material under the
 * wheel — master prompt §1.2 requires material sampling, never trigger volumes,
 * because trigger volumes duplicate track geometry, go stale when a corner
 * moves, and cannot express "two wheels on gravel".
 */
USTRUCT(BlueprintType)
struct MIDANCORE_API FMidanWheelState
{
	GENERATED_BODY()

	/**
	 * Longitudinal slip ratio, dimensionless.
	 *
	 * Positive under acceleration (wheel spinning faster than road speed),
	 * negative under braking (wheel slower). Traction control reads the positive
	 * side, ABS the negative.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	float SlipRatio = 0.f;

	/** Lateral slip angle, degrees. The oversteer/understeer signal, and what
	 *  drives tyre scrub audio and smoke. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	float SlipAngleDegrees = 0.f;

	/** Normal load through the contact patch, N. Load transfer made visible —
	 *  a wheel at near-zero load has no grip regardless of its friction value. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	float NormalLoadN = 0.f;

	/** Suspension compression, 0 fully extended to 1 fully compressed. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	float SuspensionCompression = 0.f;

	/** Angular velocity, rad/s. Signed by rotation direction. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	float AngularVelocity = 0.f;

	/** Surface under this wheel, from the physical material. Defaults to an
	 *  empty tag when the wheel is airborne — an empty tag means "unknown", not
	 *  "tarmac", and consumers must not treat it as a default surface. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	FGameplayTag SurfaceTag;

	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	bool bInContact = false;

	/** True when this wheel's surface is not a racing surface. Set by the
	 *  surface sensor from the surface response asset, so "what counts as
	 *  off-track" stays data-driven rather than a hardcoded tag list. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	bool bOffTrack = false;
};

/**
 * Whole-vehicle physics state for one sample.
 *
 * Read by MidanTelemetry at a fixed 60Hz through IMidanTelemetrySource, by
 * MidanRace for position and off-track logic, and by the feel layer for camera
 * and audio. Filled from the physics callback, so it is deliberately flat POD.
 *
 * The wheel array is a fixed-size C array, not a TArray: this struct is written
 * inside the physics callback and a TArray would allocate on first write.
 * Fixed-size UPROPERTY arrays are not Blueprint-exposable, hence the
 * GetWheel accessor on the pawn rather than direct Blueprint access.
 */
USTRUCT()
struct MIDANCORE_API FMidanVehicleFrameState
{
	GENERATED_BODY()

	/** Seconds since capture began. Not world time — telemetry needs a
	 *  monotonic clock that survives pause and time dilation. */
	UPROPERTY()
	double TimestampSeconds = 0.0;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	/** cm/s, world space. */
	UPROPERTY()
	FVector LinearVelocity = FVector::ZeroVector;

	/** rad/s, world space. */
	UPROPERTY()
	FVector AngularVelocity = FVector::ZeroVector;

	/** Signed forward speed, km/h. Negative in reverse. */
	UPROPERTY()
	float ForwardSpeedKmh = 0.f;

	UPROPERTY()
	float EngineRPM = 0.f;

	/** 0 is neutral, negative is reverse. */
	UPROPERTY()
	int32 Gear = 0;

	/** Lateral acceleration in g. The cornering-load number a driver feels. */
	UPROPERTY()
	float LateralG = 0.f;

	UPROPERTY()
	float LongitudinalG = 0.f;

	/**
	 * Chassis slip angle, degrees — the angle between where the car points and
	 * where it is actually going.
	 *
	 * Distinct from per-wheel slip angle and more useful for feel: this is what
	 * the camera's slip yaw reads (ART_DIRECTION §5, ±6°) so that a drift is
	 * legible on screen rather than looking like a bug.
	 */
	UPROPERTY()
	float ChassisSlipAngleDegrees = 0.f;

	UPROPERTY()
	FMidanWheelState Wheels[MidanVehicleConstants::NumWheels];

	/** Count of wheels currently off the racing surface. Race-side off-track
	 *  logic thresholds on this rather than on any single wheel. */
	int32 GetOffTrackWheelCount() const
	{
		int32 Count = 0;
		for (int32 i = 0; i < MidanVehicleConstants::NumWheels; ++i)
		{
			Count += Wheels[i].bOffTrack ? 1 : 0;
		}
		return Count;
	}

	/** Count of wheels in contact with anything. Zero means fully airborne. */
	int32 GetGroundedWheelCount() const
	{
		int32 Count = 0;
		for (int32 i = 0; i < MidanVehicleConstants::NumWheels; ++i)
		{
			Count += Wheels[i].bInContact ? 1 : 0;
		}
		return Count;
	}

	/** Largest absolute slip angle across the two front wheels, degrees. */
	float GetFrontAxleSlipAngle() const
	{
		return FMath::Max(
			FMath::Abs(Wheels[MidanVehicleConstants::WheelFrontLeft].SlipAngleDegrees),
			FMath::Abs(Wheels[MidanVehicleConstants::WheelFrontRight].SlipAngleDegrees));
	}

	float GetRearAxleSlipAngle() const
	{
		return FMath::Max(
			FMath::Abs(Wheels[MidanVehicleConstants::WheelRearLeft].SlipAngleDegrees),
			FMath::Abs(Wheels[MidanVehicleConstants::WheelRearRight].SlipAngleDegrees));
	}
};

/**
 * Where a vehicle is on the track.
 *
 * Arc length along the spline plus lap count is the whole basis of race
 * position — master prompt §3.3 requires it computed at 10Hz, not per frame.
 * Kept in MidanCore so MidanAI can read a racer's progress without depending
 * on MidanRace.
 */
USTRUCT(BlueprintType)
struct MIDANCORE_API FMidanTrackPosition
{
	GENERATED_BODY()

	/** Distance along the track spline, cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Track")
	float ArcLength = 0.f;

	/** Completed laps. Zero on the opening lap. */
	UPROPERTY(BlueprintReadOnly, Category = "Track")
	int32 LapIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Track")
	int32 SectorIndex = 0;

	/** Signed offset from the spline centreline, cm. Positive is right of the
	 *  direction of travel. The AI's overtaking lanes are expressed in this. */
	UPROPERTY(BlueprintReadOnly, Category = "Track")
	float LateralOffset = 0.f;

	/** Total progress for ordering racers. Monotonic across laps, so comparing
	 *  two racers is a single float comparison rather than a lap-then-arc
	 *  special case that gets the start/finish line wrong. */
	float GetTotalProgress(const float TrackLength) const
	{
		return static_cast<float>(LapIndex) * TrackLength + ArcLength;
	}
};
