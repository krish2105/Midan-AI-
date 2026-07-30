// Tyres, per axle. Your understeer/oversteer balance lives here.
//
// Responsibility: tyre friction and slip response per axle.
// Single reason to change: the Chaos wheel friction parameter surface changes.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "VehicleTyreConfig.generated.h"

struct FMidanValidationResult;

USTRUCT(BlueprintType)
struct MIDANVEHICLE_API FVehicleAxleTyreConfig
{
	GENERATED_BODY()

	/**
	 * Multiplier on the physical material's friction.
	 *
	 * Handling consequence: absolute grip level for this axle. The FRONT/REAR
	 * RATIO matters far more than either absolute value — see
	 * FVehicleTyreConfig::GetFrontRearFrictionRatio.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grip", meta = (ClampMin = "0.01", UIMax = "5.0"))
	float FrictionForceMultiplier = 2.0f;

	/**
	 * Lateral force coefficient against slip angle in degrees.
	 *
	 * Handling consequence: this SHAPE controls how the limit ARRIVES, which is
	 * what players actually feel.
	 *   Sharp peak, steep fall-off  -> knife-edge car that snaps without warning.
	 *   Rounded peak, gentle fall-off -> progressive, catchable slides.
	 *
	 * "Forgiving at the limit" means the gentle fall-off, and for a drift car
	 * that shape matters more than the absolute grip level.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grip")
	FRuntimeFloatCurve LateralSlipGraph;

	/** Lateral force per degree of slip in the linear region. Higher responds
	 *  more sharply to small steering inputs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grip", meta = (ClampMin = "100.0", UIMax = "3000.0"))
	float CorneringStiffness = 1000.f;

	/** Longitudinal slip ratio at which traction control intervenes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thresholds", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float SlipThreshold = 0.2f;

	/** Slip at which skid audio, smoke and decals begin. Purely a feedback
	 *  threshold — it does not affect the physics. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thresholds", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float SkidThreshold = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wheel", meta = (ClampMin = "10.0", UIMax = "60.0"))
	float WheelRadiusCm = 34.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wheel", meta = (ClampMin = "5.0", UIMax = "50.0"))
	float WheelWidthCm = 24.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wheel", meta = (ClampMin = "1.0"))
	float WheelMassKg = 20.f;

	/** Peak brake torque for this axle, Nm. Front should exceed rear —
	 *  rear-biased braking makes the car unstable under heavy braking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brakes", meta = (ClampMin = "0.0"))
	float MaxBrakeTorque = 3000.f;

	/** Handbrake torque. Rear axle only in practice; leave front at zero. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brakes", meta = (ClampMin = "0.0"))
	float MaxHandbrakeTorque = 0.f;

	void Validate(FName Context, FMidanValidationResult& Result) const;
};

USTRUCT(BlueprintType)
struct MIDANVEHICLE_API FVehicleTyreConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tyres")
	FVehicleAxleTyreConfig Front;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tyres")
	FVehicleAxleTyreConfig Rear;

	/**
	 * The understeer/oversteer balance as ONE number.
	 *
	 *   > 1.0  front grip exceeds rear -> OVERSTEER. Exciting, expressive,
	 *          punishing. The GT car.
	 *   < 1.0  rear grip exceeds front -> UNDERSTEER. Safe, forgiving, dull.
	 *   = 1.0  neutral.
	 *
	 * Exposed as a derived accessor rather than a stored field so it cannot
	 * disagree with the two multipliers it is computed from. This is the value
	 * to sweep in a tuning pass — sweeping one number beats sweeping two
	 * correlated ones, and it is the number that actually describes the car.
	 */
	float GetFrontRearFrictionRatio() const
	{
		return (Rear.FrictionForceMultiplier > KINDA_SMALL_NUMBER)
			? (Front.FrictionForceMultiplier / Rear.FrictionForceMultiplier)
			: 0.f;
	}

	/** Front share of total brake torque. Below ~0.5 is unstable under braking. */
	float GetFrontBrakeBias() const
	{
		const float Total = Front.MaxBrakeTorque + Rear.MaxBrakeTorque;
		return (Total > KINDA_SMALL_NUMBER) ? (Front.MaxBrakeTorque / Total) : 0.f;
	}

	void Validate(FName Context, FMidanValidationResult& Result) const;
};
