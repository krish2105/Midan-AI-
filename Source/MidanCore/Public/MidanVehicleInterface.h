// What a racer must expose to the rest of the project.
//
// Responsibility: the contract between a vehicle and everything that drives or
// observes one.
// Single reason to change: a consumer needs vehicle state it cannot derive.
//
// This interface is why MidanRace and MidanTelemetry do not depend on
// MidanVehicle (docs/ASSUMPTIONS.md A7, A8). It is also the enforcement point
// for the hard rule in master prompt §4.2: the AI must drive through the same
// input path as the player. ApplyInput is that path, and there is no other.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MidanCoreTypes.h"
#include "MidanVehicleInterface.generated.h"

struct FGameplayTag;

UINTERFACE(MinimalAPI, BlueprintType)
class UMidanVehicleInterface : public UInterface
{
	GENERATED_BODY()
};

class MIDANCORE_API IMidanVehicleInterface
{
	GENERATED_BODY()

public:
	/**
	 * THE ONLY WAY TO DRIVE A VEHICLE.
	 *
	 * The player's input component and the AI opponent controller both call
	 * this and nothing else. Implementations must not expose an alternative
	 * route that bypasses it — no direct throttle setter, no "AI mode" branch
	 * inside the movement component. Master prompt §4.2 requires that the AI
	 * cannot cheat physics, and a single entry point is how that is guaranteed
	 * structurally instead of by convention.
	 *
	 * Input arrives already shaped: Steer is post-steering-curve, not raw stick
	 * deflection. Implementations sanitise but must not re-shape.
	 */
	UFUNCTION(BlueprintCallable, Category = "Midan|Vehicle")
	virtual void ApplyInput(const FMidanVehicleInputState& Input) = 0;

	/**
	 * Current physics state.
	 *
	 * Written by reference rather than returned by value because telemetry calls
	 * this at 60Hz for up to eight vehicles and the struct is large enough that
	 * the copy is worth avoiding.
	 *
	 * Const, and must remain so: an observer must never be able to perturb the
	 * simulation by looking at it.
	 */
	virtual void GetVehicleFrameState(FMidanVehicleFrameState& OutState) const = 0;

	/** Vehicle.Class.Hyper / GT / Rally. Difficulty scaling reads this so an
	 *  opponent is never given power beyond the player's class. */
	virtual FGameplayTag GetVehicleClassTag() const = 0;

	/** Signed forward speed in km/h, negative in reverse. Separate from the
	 *  full frame state because the HUD and the camera want only this, every
	 *  frame, and should not pay for the rest. */
	UFUNCTION(BlueprintCallable, Category = "Midan|Vehicle")
	virtual float GetForwardSpeedKmh() const = 0;

	/** The most recent input applied. Telemetry records this alongside physics
	 *  state so a lap can be replayed from inputs (master prompt §6.2), and the
	 *  ghost player needs to read back what it wrote to verify resync. */
	virtual const FMidanVehicleInputState& GetLastAppliedInput() const = 0;

	/**
	 * True when the vehicle should ignore input entirely.
	 *
	 * Set during Race.State.Grid and Race.State.Countdown. Implementations zero
	 * throttle and hold the brake rather than merely dropping input — a car left
	 * to roll on a sloped grid before the lights is a bug players notice
	 * immediately.
	 */
	virtual bool IsInputLocked() const = 0;

	virtual void SetInputLocked(bool bLocked) = 0;
};
