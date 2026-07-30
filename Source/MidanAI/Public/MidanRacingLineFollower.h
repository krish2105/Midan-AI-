// Pure-pursuit lateral control with speed-dependent lookahead.
//
// Responsibility: turn "where is the racing line relative to me" into a
// normalised steering value.
// Single reason to change: the lateral control law changes.
//
// Plain UObject, not a component — it holds no per-frame state that needs
// Tick or a scene attachment, only a cached racing-line reference and gains
// copied from the difficulty asset at Initialise. AMidanOpponentController
// owns one instance and calls ComputeSteering once per control update.
//
// Pure pursuit: aim for a point LookaheadDistanceCm ahead on the racing line,
// and steer proportionally to the heading angle toward it. Lookahead grows
// with speed — a fixed lookahead either oscillates at low speed (aiming too
// far ahead to react) or clips corners at high speed (aiming too close to see
// them coming), and FVehicleSteeringConfig on the player side solves the
// equivalent problem with a speed-indexed curve for the same reason.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MidanRacingLineFollower.generated.h"

class AMidanRacingLineSpline;
class UMidanAIDifficultyDataAsset;

UCLASS()
class MIDANAI_API UMidanRacingLineFollower : public UObject
{
	GENERATED_BODY()

public:
	void Initialise(const AMidanRacingLineSpline* InRacingLine, const UMidanAIDifficultyDataAsset* InDifficulty);

	/**
	 * Normalised steering, -1..1, already in the same "final, curve-shaped
	 * value" domain IMidanVehicleInterface::ApplyInput requires — see
	 * FMidanVehicleInputState's contract in MidanCoreTypes.h.
	 *
	 * LateralOffsetCm shifts the aim point sideways from the racing line's
	 * own centre — this is how UMidanOvertakeComponent and
	 * UMidanAvoidanceComponent influence steering without this class knowing
	 * either exists: they hand the controller an offset, the controller
	 * passes it through here.
	 *
	 * Does NOT internally rate-limit — the caller applies
	 * MidanMath::RateLimitedApproach using SteeringRateLimitPerSecond, because
	 * the rate limiter's state (the previous output) lives with the control
	 * loop's other per-frame state, not inside a stateless steering
	 * calculation.
	 */
	float ComputeSteering(const FVector& VehicleLocation, const FVector& VehicleForward, float ForwardSpeedKmh, float LateralOffsetCm) const;

	/** Current arc length along the racing line closest to VehicleLocation.
	 *  Cheap-ish (one spline query) so callers cache it per control update
	 *  rather than re-deriving it. */
	float GetArcLengthAtLocation(const FVector& VehicleLocation) const;

private:
	float ComputeLookaheadDistanceCm(float ForwardSpeedKmh) const;

	UPROPERTY(Transient)
	TObjectPtr<const AMidanRacingLineSpline> RacingLine;

	float SteeringResponseGain = 1.f;
	float MinLookaheadDistanceCm = 800.f;
	float MaxLookaheadDistanceCm = 3000.f;
	float LookaheadSpeedNormalisationKmh = 250.f;
};
