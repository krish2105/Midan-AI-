// Drivetrain layout and torque distribution.
//
// Responsibility: where engine torque goes.
// Single reason to change: the Chaos differential parameter surface changes.

#pragma once

#include "CoreMinimal.h"
#include "VehicleDrivetrainConfig.generated.h"

struct FMidanValidationResult;

/** Mirrors Chaos EVehicleDifferential. Declared locally so the config struct
 *  does not force every consumer to include the Chaos header. */
UENUM(BlueprintType)
enum class EMidanDrivetrainLayout : uint8
{
	/** Stability and traction off the line. The point-and-shoot car. */
	AllWheelDrive,

	/** Pulls the nose wide under power. Distinctive, rarely what a racing
	 *  slice wants. */
	FrontWheelDrive,

	/** High torque plus RWD equals oversteer character. The drift car. */
	RearWheelDrive
};

USTRUCT(BlueprintType)
struct MIDANVEHICLE_API FVehicleDrivetrainConfig
{
	GENERATED_BODY()

	/**
	 * Handling consequence: the most instantly legible character lever there
	 * is. A player identifies RWD versus AWD within one corner, before they
	 * could describe any other difference.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drivetrain")
	EMidanDrivetrainLayout Layout = EMidanDrivetrainLayout::RearWheelDrive;

	/**
	 * Fraction of torque to the FRONT axle. AWD only.
	 *
	 * Handling consequence: a stronger character lever than most people expect.
	 * The same chassis at 0.4 and 0.6 feels like two different cars — rearward
	 * bias rotates on throttle, forward bias pulls straight.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drivetrain",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "Layout == EMidanDrivetrainLayout::AllWheelDrive"))
	float FrontTorqueSplit = 0.4f;

	/** Left/right split on the front axle. 0.5 is even. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drivetrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FrontLeftRightSplit = 0.5f;

	/** Left/right split on the rear axle. 0.5 is even. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drivetrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RearLeftRightSplit = 0.5f;

	/**
	 * Locking strength, 0 open to 1 fully locked.
	 *
	 * Handling consequence: a locked rear diff makes power-on oversteer
	 * predictable and sustained — which is what makes a slide holdable rather
	 * than a spin. An open diff spins the inside wheel and the slide dies.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drivetrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RearDifferentialLock = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drivetrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FrontDifferentialLock = 0.2f;

	void Validate(FName Context, FMidanValidationResult& Result) const;
};
