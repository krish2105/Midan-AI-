// Aerodynamics. Chaos does not provide usable downforce, so we apply it
// ourselves in the async physics callback.
//
// Responsibility: drag and downforce coefficients and application geometry.
// Single reason to change: the aero force model changes.
//
// The separate front and rear application points are the whole value of doing
// this by hand. Front/rear downforce balance shifting with speed is what makes
// a car feel planted at 250km/h and nervous at 60 — a speed-dependent handling
// personality from four numbers, which no amount of suspension tuning imitates.

#pragma once

#include "CoreMinimal.h"
#include "VehicleAeroConfig.generated.h"

struct FMidanValidationResult;

USTRUCT(BlueprintType)
struct MIDANVEHICLE_API FVehicleAeroConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aero")
	bool bEnabled = true;

	/**
	 * Drag coefficient Cd, dimensionless.
	 *
	 * Handling consequence: sets top speed and how hard the car decelerates off
	 * throttle at speed. Roughly 0.30 for a slippery hypercar, 0.35 for a GT,
	 * 0.40+ for something with a big wing or a rally stance.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aero|Drag", meta = (ClampMin = "0.0", UIMax = "1.5"))
	float DragCoefficient = 0.33f;

	/** Reference frontal area, m². About 2.0-2.3 for a car this size. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aero|Drag", meta = (ClampMin = "0.1", UIMax = "5.0"))
	float FrontalAreaM2 = 2.1f;

	/**
	 * Front downforce coefficient Cl.
	 *
	 * Handling consequence: front grip that ARRIVES WITH SPEED. High front Cl
	 * makes high-speed corner entry bite; too much relative to the rear makes
	 * the car snap at speed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aero|Downforce", meta = (ClampMin = "0.0", UIMax = "5.0"))
	float FrontLiftCoefficient = 0.6f;

	/**
	 * Rear downforce coefficient Cl.
	 *
	 * Handling consequence: rear stability at speed. Rear-biased downforce is
	 * what makes the hypercar "planted above 150km/h, nervous below 80" — at
	 * low speed there is no aero, so the mechanical balance shows through.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aero|Downforce", meta = (ClampMin = "0.0", UIMax = "5.0"))
	float RearLiftCoefficient = 1.0f;

	/** Drag application point, cm from chassis origin. Above the COM produces a
	 *  slight nose-down pitch at speed, which is realistic and free. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aero|Geometry")
	FVector CentreOfPressureOffset = FVector(0.f, 0.f, 40.f);

	/** Front downforce application point, cm from chassis origin. Should sit
	 *  ahead of the COM or it contributes nothing to front-end bite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aero|Geometry")
	FVector FrontApplicationOffset = FVector(150.f, 0.f, 20.f);

	/** Rear downforce application point, cm from chassis origin. Behind the COM. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aero|Geometry")
	FVector RearApplicationOffset = FVector(-150.f, 0.f, 60.f);

	/** Speed below which downforce is ignored, km/h. Below this v² is
	 *  negligible anyway, and skipping it avoids pointless callback work. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aero", meta = (ClampMin = "0.0", UIMax = "100.0"))
	float MinSpeedForDownforceKmh = 20.f;

	/** Safety clamp on total downforce as a multiple of vehicle weight.
	 *  Not a tuning dial — it stops a mis-authored coefficient from pinning the
	 *  car to the ground or destabilising the solver. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aero", meta = (ClampMin = "0.1", UIMax = "10.0"))
	float MaxDownforceAsWeightMultiple = 3.f;

	/** Aero balance as one number: front share of total downforce. 0.5 is
	 *  neutral, below 0.5 is rear-biased and stable at speed. */
	float GetFrontDownforceBalance() const
	{
		const float Total = FrontLiftCoefficient + RearLiftCoefficient;
		return (Total > KINDA_SMALL_NUMBER) ? (FrontLiftCoefficient / Total) : 0.5f;
	}

	void Validate(FName Context, FMidanValidationResult& Result) const;
};
