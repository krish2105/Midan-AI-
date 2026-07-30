// Thin subclass of the Chaos wheeled movement component.
//
// Responsibility: expose the async physics hook and per-substep assist
// application to the rest of the vehicle.
// Single reason to change: the Chaos movement component's extension surface
// changes.
//
// DELIBERATELY THIN. This class adds an assist hook and a slip-angle accessor
// and nothing else. Everything tunable is written into it by
// UVehicleSetupApplier from the Data Asset, so this class holds no tuning values
// of its own — that is what makes the car diffable in version control and
// sweepable programmatically.
//
// It exists at all because the assists must run per-substep, between reading
// slip and the solver consuming input. There is no way to do that from outside
// the movement component.

#pragma once

#include "CoreMinimal.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "MidanCoreTypes.h"
#include "MidanVehicleMovementComponent.generated.h"

class UVehicleAssistComponent;

UCLASS(ClassGroup = (Midan), meta = (BlueprintSpawnableComponent))
class MIDANVEHICLE_API UMidanVehicleMovementComponent : public UChaosWheeledVehicleMovementComponent
{
	GENERATED_BODY()

public:
	UMidanVehicleMovementComponent();

	/**
	 * Route commanded input into the solver.
	 *
	 * The single point at which input reaches Chaos. Assists are applied here,
	 * per-substep, before the base class consumes the values — which is why they
	 * live in a movement-component override rather than in the pawn's tick.
	 *
	 * Input arrives already curve-shaped from UVehicleInputComponent or already
	 * normalised from the AI. This method does not shape, it only forwards.
	 */
	void SetMidanInput(const FMidanVehicleInputState& Input);

	/** Assists are applied by this component because they need per-substep slip.
	 *  Set once by the pawn. */
	void SetAssistComponent(UVehicleAssistComponent* InAssists) { Assists = InAssists; }

	/**
	 * Chassis slip angle in degrees — the angle between where the car points and
	 * where it is going.
	 *
	 * Distinct from per-wheel slip angle and more useful for feel: the camera's
	 * slip yaw reads this (ART_DIRECTION §5, ±6°) so a drift is legible on
	 * screen rather than looking like a rendering bug.
	 *
	 * Positive when sliding right relative to the nose.
	 */
	UFUNCTION(BlueprintPure, Category = "Midan|Vehicle")
	float GetChassisSlipAngleDegrees() const;

	/** Signed forward speed in km/h. Negative in reverse. */
	UFUNCTION(BlueprintPure, Category = "Midan|Vehicle")
	float GetForwardSpeedKmh() const;

	/** Lateral and longitudinal acceleration in g, from finite-differenced
	 *  velocity. The numbers a driver actually feels, and what the HUD's g-meter
	 *  and telemetry record. */
	void GetAccelerationG(float& OutLateralG, float& OutLongitudinalG) const;

	/** Fill the per-wheel physics fields of a frame state. Surface fields are
	 *  written separately by UVehicleSurfaceSensorComponent, so neither
	 *  component needs to know about the other. */
	void WriteWheelPhysicsToFrameState(FMidanVehicleFrameState& OutState) const;

	//~ UActorComponent
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVehicleAssistComponent> Assists;

	/** Input as commanded, before assists. Kept so telemetry can record what the
	 *  driver asked for alongside what the car did — the difference between the
	 *  two is how you prove an assist is behaving. */
	FMidanVehicleInputState CommandedInput;

	/** Input after assists, as actually fed to the solver. */
	FMidanVehicleInputState EffectiveInput;

	//~ Acceleration state, finite-differenced on the game thread.
	FVector PreviousVelocity = FVector::ZeroVector;
	float CachedLateralG = 0.f;
	float CachedLongitudinalG = 0.f;
};
