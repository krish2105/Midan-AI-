// Suspension, per axle. Where amateur racing games fail.
//
// Responsibility: spring and damper parameters per axle.
// Single reason to change: the Chaos suspension parameter surface changes.
//
// BODY ROLL IS HOW THE PLAYER READS GRIP. The player cannot see a friction
// coefficient; they see the car lean and infer the limit from it. Suppress the
// lean and you have removed their only instrument.

#pragma once

#include "CoreMinimal.h"
#include "VehicleSuspensionConfig.generated.h"

struct FMidanValidationResult;

USTRUCT(BlueprintType)
struct MIDANVEHICLE_API FVehicleAxleSuspensionConfig
{
	GENERATED_BODY()

	/** Travel above rest, cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Travel", meta = (ClampMin = "0.0", UIMax = "30.0"))
	float MaxRaiseCm = 8.f;

	/**
	 * Travel below rest, cm.
	 *
	 * Handling consequence: too little total travel makes every bump an impact
	 * and the car skates rather than absorbing. The rally car's high travel is
	 * the single biggest reason it reads as a rally car.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Travel", meta = (ClampMin = "0.0", UIMax = "30.0"))
	float MaxDropCm = 8.f;

	/** Spring stiffness. Scale against MassKg — a rate that suits 1250kg will
	 *  wallow at 1550kg. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spring", meta = (ClampMin = "1.0"))
	float SpringRate = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpringPreload = 0.5f;

	/**
	 * Damping ratio. ~1.0 is critically damped.
	 *
	 * Handling consequence: the most common way to ruin a car.
	 *   OVER-damped  -> fast car that feels DEAD. Numbers look good, nobody
	 *                   enjoys driving it. This is what you get when you "fix"
	 *                   instability with damping instead of finding its cause.
	 *   UNDER-damped -> a boat. Oscillates after every input, feels disconnected.
	 *
	 * Tune by watching the car settle after a kerb: one clear compression and
	 * rebound, then settled. Two or three oscillations is under-damped; no
	 * visible movement at all is over-damped.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spring", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float DampingRatio = 0.7f;

	/** Vertical offset of the force application point from the wheel, cm.
	 *  Raising it increases roll resistance without stiffening the spring. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spring")
	float ForceOffsetCm = 0.f;

	/** Fraction of chassis mass this axle carries. Front+rear should total 1.0. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Load", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WheelLoadRatio = 0.5f;

	/**
	 * Anti-roll bar stiffness, 0 to 1.
	 *
	 * Handling consequence: the balance dial that does not cost ride quality.
	 * Stiffening the front bar adds understeer; stiffening the rear adds
	 * oversteer. Reach for this before changing spring rates.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AntiRollBarStiffness = 0.f;

	void Validate(FName Context, FMidanValidationResult& Result) const;
};

USTRUCT(BlueprintType)
struct MIDANVEHICLE_API FVehicleSuspensionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suspension")
	FVehicleAxleSuspensionConfig Front;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suspension")
	FVehicleAxleSuspensionConfig Rear;

	void Validate(FName Context, FMidanValidationResult& Result) const;
};
