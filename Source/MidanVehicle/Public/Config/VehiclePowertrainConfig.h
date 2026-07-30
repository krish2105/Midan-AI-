// Engine and gearbox. The difference between fast and *feeling* fast.
//
// Responsibility: torque production and gear selection parameters.
// Single reason to change: the Chaos engine/transmission parameter surface changes.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "VehiclePowertrainConfig.generated.h"

struct FMidanValidationResult;

USTRUCT(BlueprintType)
struct MIDANVEHICLE_API FVehiclePowertrainConfig
{
	GENERATED_BODY()

	/**
	 * Torque in Nm against engine RPM.
	 *
	 * Handling consequence: the most expressive parameter available. PEAKY
	 * curves feel dramatic; FLAT curves feel fast but dull. A car that pulls
	 * hard at 6000rpm is memorable; one with identical lap time and a flat
	 * curve is not. Per VEHICLE_SPEC: peaky for the hypercar, flat and wide for
	 * the GT, torquey low-end for the rally car.
	 *
	 * The domain must cover [EngineIdleRPM, MaxRPM] — a curve stopping short of
	 * MaxRPM produces a car that mysteriously dies at the top end.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Engine")
	FRuntimeFloatCurve TorqueCurve;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Engine", meta = (ClampMin = "1000.0", UIMax = "12000.0"))
	float MaxRPM = 7500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Engine", meta = (ClampMin = "0.0", UIMax = "2000.0"))
	float EngineIdleRPM = 900.f;

	/**
	 * Off-throttle engine braking strength.
	 *
	 * Handling consequence: large and underrated. Off-throttle deceleration is
	 * what makes lifting off *mean* something, and it is a major part of why a
	 * car feels connected rather than coasting.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Engine", meta = (ClampMin = "0.0", UIMax = "500.0"))
	float EngineBrakeEffect = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Engine", meta = (ClampMin = "0.0"))
	float EngineRevUpMOI = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Engine", meta = (ClampMin = "0.0"))
	float EngineRevDownRate = 600.f;

	/**
	 * Forward gear ratios, highest first. Must be monotonically decreasing.
	 *
	 * Handling consequence: gear spacing controls HOW OFTEN the player feels an
	 * event. Close ratios give frequent shifts and a sense of busyness; long
	 * ratios give fewer, bigger moments. This is pacing, not just acceleration.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission")
	TArray<float> ForwardGearRatios = { 3.2f, 2.1f, 1.5f, 1.15f, 0.92f, 0.75f };

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission", meta = (ClampMin = "0.1"))
	float ReverseGearRatio = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission", meta = (ClampMin = "0.1"))
	float FinalDriveRatio = 3.6f;

	/**
	 * Shift durations in seconds.
	 *
	 * Handling consequence: shift punch. Sub-0.15s reads as a modern
	 * dual-clutch. Above ~0.4s reads as an old automatic, which is a valid
	 * choice if you mean it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float ChangeUpTime = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float ChangeDownTime = 0.15f;

	/** Fraction of MaxRPM at which an automatic gearbox shifts up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float AutoShiftUpRatio = 0.92f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AutoShiftDownRatio = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission")
	bool bUseAutomaticGears = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TransmissionEfficiency = 0.94f;

	void Validate(FName Context, FMidanValidationResult& Result) const;
};
