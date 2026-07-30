// Steering. NEVER map raw input directly to steer angle.
//
// Responsibility: steering geometry limits and input shaping.
// Single reason to change: the input shaping model changes.
//
// Raw stick or key input mapped straight to steer angle is the single most
// common reason a UE5 vehicle "feels wrong": full lock at 250km/h is an
// instant spin, and the angle that feels right in a hairpin is unusable on a
// straight. The SteeringCurve is mandatory, not optional.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "VehicleSteeringConfig.generated.h"

struct FMidanValidationResult;

USTRUCT(BlueprintType)
struct MIDANVEHICLE_API FVehicleSteeringConfig
{
	GENERATED_BODY()

	/** Steer angle at full lock and zero speed, degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "5.0", ClampMax = "70.0"))
	float MaxSteerAngleDegrees = 40.f;

	/**
	 * MANDATORY. Maximum steer angle multiplier (0..1) against forward speed in km/h.
	 *
	 * Handling consequence: this curve is the difference between a car that
	 * feels precise and one that feels twitchy at speed and vague at parking
	 * pace. Must be monotonically non-increasing — permitting more lock at
	 * higher speed is never correct, and validation errors on it.
	 *
	 * Domain must cover [0, ValidationTopSpeedKmh].
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering")
	FRuntimeFloatCurve SteeringCurve;

	/**
	 * Separate shaping for DIGITAL input (keyboard).
	 *
	 * Maps time-held in seconds to steer magnitude 0..1. Binary keys through a
	 * linear map is a distinct failure from gamepad tuning and needs its own
	 * curve, not a scaled copy of the analogue one. Ramp in over roughly
	 * 100-200ms. See .claude/skills/game-feel-engineering §2.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering|Keyboard")
	FRuntimeFloatCurve KeyboardShapingCurve;

	/**
	 * Rate of steer increase toward the target, units/sec.
	 *
	 * Handling consequence: fast rise with slower fall feels responsive but
	 * stable. One symmetric rate is a compromise that satisfies neither.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering|Rates", meta = (ClampMin = "0.1", UIMax = "20.0"))
	float SteeringInputRiseRate = 6.f;

	/** Rate of steer return toward centre, units/sec. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering|Rates", meta = (ClampMin = "0.1", UIMax = "20.0"))
	float SteeringInputFallRate = 8.f;

	/**
	 * Ackermann geometry accuracy, 0 parallel to 1 fully correct.
	 *
	 * Handling consequence: matters at low speed and full lock, and almost
	 * nowhere else. Cheap correctness.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AckermannAccuracy = 1.f;

	/** Analogue stick deadzone. Hardware compensation, not feel tuning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering|Analogue", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float AnalogueDeadzone = 0.05f;

	/**
	 * Top speed used to validate the SteeringCurve domain, km/h.
	 *
	 * Not a physics limit — the car's real top speed comes from the powertrain,
	 * aero and gearing. This exists so validation knows how far the curve must
	 * reach, and it must be set at or above the achievable top speed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "50.0", UIMax = "500.0"))
	float ValidationTopSpeedKmh = 340.f;

	void Validate(FName Context, FMidanValidationResult& Result) const;
};
