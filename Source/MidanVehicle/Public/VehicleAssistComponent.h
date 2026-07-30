// Traction control, ABS, stability control and steering assist.
//
// Responsibility: modify commanded input based on measured slip.
// Single reason to change: an assist is added or its intervention model changes.
//
// Each assist is INDEPENDENTLY toggleable and every gain comes from
// FVehicleAssistConfig. None of them is a hardcoded float, because each is
// exactly the kind of value a designer tunes by feel — and because the three
// cars want different assist behaviour: the GT is the drift car and wants
// stability control off, while the hypercar wants traction control to make its
// 800hp usable.
//
// ASSISTS MODIFY INPUT, THEY DO NOT APPLY FORCE. They run in the physics
// callback because they read per-substep slip — reading slip at frame rate would
// make an assist that behaves differently at 30fps and 144fps — but their output
// is a modified FMidanVehicleInputState, which then goes through the same single
// ApplyInput door as everything else. An assist that applied a corrective force
// directly would be a second path into the solver, and master prompt §4.2 allows
// exactly one.
//
// ZERO ALLOCATION. Fixed-size state, no containers, no logging in the steady
// path (assist logging is behind a developer-settings toggle and off by default,
// because traction control fires many times per second).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Config/VehicleAssistConfig.h"
#include "MidanCoreTypes.h"
#include "VehicleAssistComponent.generated.h"

class UChaosWheeledVehicleMovementComponent;

/** What intervened on the last evaluation. Bit flags so the HUD can show a
 *  single combined indicator and telemetry can record which assist fired. */
UENUM(BlueprintType, meta = (Bitflags))
enum class EMidanAssistFlags : uint8
{
	None            = 0,
	TractionControl = 1 << 0,
	ABS             = 1 << 1,
	Stability       = 1 << 2,
	SteeringAssist  = 1 << 3
};
ENUM_CLASS_FLAGS(EMidanAssistFlags);

UCLASS(ClassGroup = (Midan), meta = (BlueprintSpawnableComponent))
class MIDANVEHICLE_API UVehicleAssistComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleAssistComponent();

	/** Config copied by value — same reasoning as UVehicleAeroComponent. */
	void InitialiseFromConfig(const FVehicleAssistConfig& InConfig, UChaosWheeledVehicleMovementComponent* InMovement);

	/**
	 * Apply every enabled assist to a commanded input.
	 *
	 * Called from the physics callback with the input the driver (or AI) asked
	 * for; returns what the car should actually do. Assists are applied in a
	 * fixed order — see the implementation for why the order matters.
	 *
	 * Reads slip from the movement component's per-wheel state.
	 */
	void ApplyAssists(FMidanVehicleInputState& InOutInput, float ForwardSpeedKmh, float ChassisSlipAngleDegrees);

	/** Runtime override, for the settings menu. Data supplies the defaults; the
	 *  player may switch an assist off without editing the asset. */
	UFUNCTION(BlueprintCallable, Category = "Midan|Assists")
	void SetAssistEnabled(EMidanAssistFlags Assist, bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Midan|Assists")
	bool IsAssistEnabled(EMidanAssistFlags Assist) const;

	/** Which assists intervened on the last evaluation. */
	EMidanAssistFlags GetActiveInterventions() const { return LastInterventions; }

	/** Throttle reduction applied by traction control last evaluation, 0..1.
	 *  Exposed for the HUD's TC indicator and for telemetry. */
	float GetLastTractionControlReduction() const { return LastTCReduction; }

	float GetLastABSReduction() const { return LastABSReduction; }

private:
	/**
	 * Traction control: cut throttle when a driven wheel spins faster than road
	 * speed.
	 *
	 * Returns the throttle multiplier, 0..1. Reads the maximum positive slip
	 * ratio across driven wheels — using the average would let one spinning
	 * wheel hide behind three gripping ones, which is the case TC exists for.
	 */
	float ComputeTractionControlMultiplier() const;

	/**
	 * ABS: release brake when a wheel locks.
	 *
	 * Returns the brake multiplier, 0..1. Reads the most negative slip ratio —
	 * a single locked wheel is what causes a flat spot and a loss of steering,
	 * so the worst wheel governs.
	 */
	float ComputeABSMultiplier() const;

	/**
	 * Stability control: reduce throttle when the chassis slip angle exceeds the
	 * threshold.
	 *
	 * Deliberately crude, and deliberately OFF by default on the GT. A stability
	 * system good enough to be invisible would also remove the controllable
	 * slides that are the GT's entire personality.
	 */
	float ComputeStabilityMultiplier(float ChassisSlipAngleDegrees) const;

	/**
	 * Steering assist: nudge steer toward the direction of travel during a slide.
	 *
	 * Counter-steer help for pad players. Strength is low by default because at
	 * high values the car starts driving itself, which reads as the controls
	 * fighting the player.
	 */
	float ComputeSteeringAssistDelta(float CurrentSteer, float ChassisSlipAngleDegrees, float ForwardSpeedKmh) const;

	FVehicleAssistConfig CachedConfig;

	UPROPERTY(Transient)
	TObjectPtr<UChaosWheeledVehicleMovementComponent> Movement;

	/** Runtime enable mask, initialised from the config's per-assist bools. */
	EMidanAssistFlags EnabledMask = EMidanAssistFlags::None;

	EMidanAssistFlags LastInterventions = EMidanAssistFlags::None;

	float LastTCReduction = 0.f;
	float LastABSReduction = 0.f;

	bool bInitialised = false;
};
