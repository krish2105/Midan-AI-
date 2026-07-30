// Enhanced Input action references for the driving context.
//
// Responsibility: hold soft references to the input assets.
// Single reason to change: a new driving input action exists.
//
// UInputAction and UInputMappingContext are editor-authored UAssets and cannot
// be created from C++, so this asset is the bridge: C++ names the actions it
// needs, a human authors them, and this asset connects the two. The exact asset
// names expected are listed in docs/MANUAL_STEPS.md §1.3.
//
// Contains NO shaping values. Deadzone lives in DefaultInput.ini (hardware
// compensation) and response shaping lives in FVehicleSteeringConfig (tuning).
// Putting a response curve modifier on the UInputAction itself would split
// handling tuning across two places, and one of them would not be diffable as a
// number — see docs/MANUAL_STEPS.md §1.3.

#pragma once

#include "CoreMinimal.h"
#include "MidanDataAsset.h"
#include "MidanInputConfigDataAsset.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS(BlueprintType)
class MIDANVEHICLE_API UMidanInputConfigDataAsset : public UMidanDataAsset
{
	GENERATED_BODY()

public:
	/** Mapping context pushed onto the local player subsystem when the pawn is
	 *  possessed, and removed when it is unpossessed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Context")
	TSoftObjectPtr<UInputMappingContext> DrivingContext;

	/** Priority for the driving context. Higher wins. Pause and UI contexts
	 *  push above this so a menu takes input precedence over the car. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Context", meta = (ClampMin = "0", ClampMax = "100"))
	int32 DrivingContextPriority = 0;

	//~ Continuous axes -----------------------------------------------------

	/** Axis1D, 0..1. Trigger or key. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions|Continuous")
	TSoftObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions|Continuous")
	TSoftObjectPtr<UInputAction> BrakeAction;

	/** Axis1D, -1..1. RAW deflection — the steering curve is applied in
	 *  UVehicleInputComponent, not here and not on the action. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions|Continuous")
	TSoftObjectPtr<UInputAction> SteerAction;

	/** Axis1D, 0..1. Analogue so a partial pull is possible on a trigger. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions|Continuous")
	TSoftObjectPtr<UInputAction> HandbrakeAction;

	//~ Digital -------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions|Digital")
	TSoftObjectPtr<UInputAction> ShiftUpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions|Digital")
	TSoftObjectPtr<UInputAction> ShiftDownAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions|Digital")
	TSoftObjectPtr<UInputAction> LookBackAction;

	/** Cycles the four camera modes (Phase 4). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions|Digital")
	TSoftObjectPtr<UInputAction> CameraModeAction;

	/** Respawn at the last valid checkpoint (Phase 5). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions|Digital")
	TSoftObjectPtr<UInputAction> RespawnAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions|Digital")
	TSoftObjectPtr<UInputAction> PauseAction;

	//~ Keyboard detection --------------------------------------------------

	/**
	 * Absolute steer deflection above which input is treated as digital.
	 *
	 * A keyboard reports full deflection instantly; a stick ramps. When raw
	 * steer arrives at magnitude 1.0 on the first frame of a press, the keyboard
	 * shaping curve applies instead of the analogue path — because binary keys
	 * through an analogue map is the single most common reason a UE5 car feels
	 * wrong (master prompt §1.2).
	 *
	 * Not a feel value: it is a device-classification threshold. The feel lives
	 * in FVehicleSteeringConfig::KeyboardShapingCurve.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Device", meta = (ClampMin = "0.9", ClampMax = "1.0"))
	float DigitalDetectionThreshold = 0.999f;

	//~ UMidanDataAsset
	virtual void ValidateMidanData(FMidanValidationResult& Result) const override;
};
