// Turns Enhanced Input into a shaped FMidanVehicleInputState.
//
// Responsibility: read raw input and shape it into the vehicle input contract.
// Single reason to change: the input shaping model changes.
//
// This component is where master prompt §1.2's most important input rule is
// enforced: raw stick or key deflection NEVER reaches steer angle directly. It
// goes through the speed-sensitive steering curve, then a rise/fall rate
// limiter, and keyboard input takes an entirely separate shaping path.
//
// It runs on the game thread at frame rate, not in the physics callback. That is
// correct and justified: input arrives as frame events and the rate limiter is
// frame-dt based (docs/ARCHITECTURE.md §2.3). The shaped result is handed to the
// pawn, which passes it to the solver — so input sampling rate and physics rate
// stay properly decoupled.
//
// Produces input. Does not apply it. The pawn owns ApplyInput.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MidanCoreTypes.h"
#include "VehicleInputComponent.generated.h"

class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UMidanInputConfigDataAsset;
struct FInputActionValue;
struct FVehicleSteeringConfig;

UCLASS(ClassGroup = (Midan), meta = (BlueprintSpawnableComponent))
class MIDANVEHICLE_API UVehicleInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleInputComponent();

	/** Input asset. Soft-loaded by the pawn before binding. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TSoftObjectPtr<UMidanInputConfigDataAsset> InputConfig;

	/**
	 * Bind actions and push the mapping context.
	 *
	 * Called by the pawn from SetupPlayerInputComponent. Requires the input
	 * config to already be loaded — the pawn resolves the soft pointer first,
	 * because CLAUDE.md forbids a synchronous load here.
	 */
	void SetupInputBindings(UEnhancedInputComponent* EnhancedInput, const UMidanInputConfigDataAsset* LoadedConfig);

	/** Remove the mapping context. Called on unpossess so an unpossessed car
	 *  does not keep consuming input. */
	void TeardownInputBindings();

	/**
	 * Shape raw input into the vehicle contract.
	 *
	 * Called once per frame by the pawn. Steering config supplies the curves and
	 * rates; forward speed selects the point on the speed-sensitive curve.
	 */
	const FMidanVehicleInputState& ShapeInput(const FVehicleSteeringConfig& SteeringConfig, float ForwardSpeedKmh, float DeltaSeconds);

	/** Last shaped result, without recomputing. */
	const FMidanVehicleInputState& GetShapedInput() const { return ShapedInput; }

	/** True when the most recent steer input looked like a keyboard. Exposed for
	 *  the HUD, which shows a different steering indicator per device. */
	UFUNCTION(BlueprintPure, Category = "Midan|Input")
	bool IsUsingDigitalSteering() const { return bDigitalSteerActive; }

protected:
	//~ Action handlers. Store raw values only — all shaping happens in
	//  ShapeInput so there is exactly one place the curves are applied.
	void OnThrottle(const FInputActionValue& Value);
	void OnBrake(const FInputActionValue& Value);
	void OnSteer(const FInputActionValue& Value);
	void OnSteerReleased(const FInputActionValue& Value);
	void OnHandbrake(const FInputActionValue& Value);
	void OnShiftUp(const FInputActionValue& Value);
	void OnShiftDown(const FInputActionValue& Value);

private:
	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem() const;

	/**
	 * Apply the speed-sensitive steering curve.
	 *
	 * Returns the maximum steer magnitude (0..1) permitted at this speed. Master
	 * prompt §1.2 makes this mandatory: full lock at 250km/h is an instant spin,
	 * and the angle that feels right in a hairpin is unusable on a straight.
	 */
	static float EvaluateSteeringLimit(const FVehicleSteeringConfig& Config, float ForwardSpeedKmh);

	/**
	 * Shape digital steer input through the keyboard curve.
	 *
	 * Keyboard input is a separate problem from analogue input, not a scaled
	 * version of it — hence its own curve keyed on time-held rather than on
	 * deflection, which is always either 0 or 1.
	 */
	float ShapeDigitalSteer(const FVehicleSteeringConfig& Config, float DeltaSeconds);

	//~ Raw values, written by the action handlers.
	float RawThrottle = 0.f;
	float RawBrake = 0.f;
	float RawSteer = 0.f;
	float RawHandbrake = 0.f;

	//~ Edge-triggered, consumed by the pawn and cleared each frame.
	bool bShiftUpPending = false;
	bool bShiftDownPending = false;

	/** Time the current digital steer direction has been held, seconds. Drives
	 *  the keyboard shaping curve's X axis. */
	float DigitalSteerHeldSeconds = 0.f;

	/** Sign of the current digital steer direction. Reset on direction change so
	 *  a left-to-right flick restarts the ramp rather than continuing it. */
	float DigitalSteerDirection = 0.f;

	bool bDigitalSteerActive = false;

	/** Rate-limited steer, persisted across frames. This is the state the
	 *  rise/fall limiter integrates — without persistence there is no limiting. */
	float SmoothedSteer = 0.f;

	FMidanVehicleInputState ShapedInput;

	UPROPERTY(Transient)
	TObjectPtr<const UMidanInputConfigDataAsset> ActiveConfig;
};
