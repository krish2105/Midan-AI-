// Mass and inertia. Set these realistically FIRST, then tune elsewhere.
//
// Responsibility: chassis mass distribution parameters.
// Single reason to change: the Chaos mass parameter surface changes.

#pragma once

#include "CoreMinimal.h"
#include "VehicleMassConfig.generated.h"

struct FMidanValidationResult;

USTRUCT(BlueprintType)
struct MIDANVEHICLE_API FVehicleMassConfig
{
	GENERATED_BODY()

	/**
	 * Chassis mass in kg, excluding wheels.
	 *
	 * Handling consequence: everything downstream. Spring rates, downforce,
	 * braking distance and tyre load all read against this. Fudging mass to fix
	 * a symptom breaks every other parameter's meaning — set it realistically
	 * and fix the symptom where it actually lives.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mass", meta = (ClampMin = "1.0", UIMin = "800.0", UIMax = "2500.0"))
	float MassKg = 1500.f;

	/**
	 * Centre of mass offset from the chassis origin, cm.
	 *
	 * Handling consequence: a DESIGN DIAL, not a bug fix. Lowering Z
	 * artificially stabilises the car and kills body movement — for the rally
	 * car that removes exactly the character it exists to have. If the car
	 * flips, check physics-asset bone weighting before touching this; bad bone
	 * weighting is the usual cause and lowering the COM only masks it.
	 *
	 * Validation warns above a threshold rather than erroring, because a high
	 * COM is sometimes the intent.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mass")
	FVector CentreOfMassOffset = FVector(0.f, 0.f, -10.f);

	/**
	 * Per-axis scale on the computed inertia tensor.
	 *
	 * Handling consequence: rotational willingness. Raise for reluctance and
	 * stability, lower for agility. The cheapest way to make two cars of
	 * similar mass feel genuinely different, and it costs nothing to tune.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mass", meta = (ClampMin = "0.1"))
	FVector InertiaTensorScale = FVector(1.f, 1.f, 1.f);

	/**
	 * Downward force scale applied while airborne, to settle the car.
	 *
	 * Handling consequence: prevents the floaty extended air time that makes a
	 * kerb strike read as a bug. Purely a feel value.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mass", meta = (ClampMin = "0.0", UIMax = "5.0"))
	float DownforceWhenAirborne = 0.f;

	void Validate(FName Context, FMidanValidationResult& Result) const;
};
