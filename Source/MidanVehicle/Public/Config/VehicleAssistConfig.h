// Driver assists. Each individually toggleable, every gain from data.
//
// Responsibility: assist enable flags and gains.
// Single reason to change: an assist is added or its control model changes.

#pragma once

#include "CoreMinimal.h"
#include "VehicleAssistConfig.generated.h"

struct FMidanValidationResult;

USTRUCT(BlueprintType)
struct MIDANVEHICLE_API FVehicleAssistConfig
{
	GENERATED_BODY()

	// --- Traction control ----------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Traction Control")
	bool bTractionControlEnabled = true;

	/** Drive torque is cut above this longitudinal slip ratio. Lower is more
	 *  intrusive — it will also make a deliberately loose car undriveable, so
	 *  the rally and GT setups want this higher than the hypercar. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Traction Control",
		meta = (ClampMin = "0.01", ClampMax = "1.0", EditCondition = "bTractionControlEnabled"))
	float TractionControlSlipThreshold = 0.25f;

	/** Fraction of torque removed per unit of slip over threshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Traction Control",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bTractionControlEnabled"))
	float TractionControlStrength = 0.6f;

	// --- ABS -----------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ABS")
	bool bABSEnabled = true;

	/** Brake torque is modulated above this slip ratio to prevent lockup. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ABS",
		meta = (ClampMin = "0.01", ClampMax = "1.0", EditCondition = "bABSEnabled"))
	float ABSSlipThreshold = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ABS",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bABSEnabled"))
	float ABSStrength = 0.8f;

	// --- Stability control ---------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stability")
	bool bStabilityControlEnabled = false;

	/**
	 * Chassis slip angle above which a corrective yaw moment is applied, degrees.
	 *
	 * Default OFF, and the threshold is high. Stability control fights the
	 * oversteer character the GT car exists to have — an assist that cancels
	 * the intended design is worse than no assist. Enable it as a player
	 * accessibility option, not as a default.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stability",
		meta = (ClampMin = "1.0", UIMax = "45.0", EditCondition = "bStabilityControlEnabled"))
	float StabilitySlipAngleThresholdDegrees = 15.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stability",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bStabilityControlEnabled"))
	float StabilityStrength = 0.4f;

	// --- Steering assist -----------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering Assist")
	bool bSteeringAssistEnabled = false;

	/** Bias applied toward the slide-recovery direction, 0..1. Keep low —
	 *  above about 0.3 the player feels the car steering itself, which reads as
	 *  the controls being taken away. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering Assist",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bSteeringAssistEnabled"))
	float SteeringAssistStrength = 0.15f;

	void Validate(FName Context, FMidanValidationResult& Result) const;
};
