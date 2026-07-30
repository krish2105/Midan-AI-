// The player's camera. Roughly the largest single feel lever in the project.
//
// Responsibility: own the camera view, mode selection and speed-driven post.
// Single reason to change: the set of camera modes or post channels changes.
//
// The camera does more for the sensation of speed and weight than the
// simulation does. A technically correct car with a rigid, unlagged, fixed-FOV
// camera feels slow and weightless; the same car with a lagging boom and a
// widening lens feels fast. That asymmetry is why this component exists as its
// own system rather than as a spring arm on the pawn.
//
// Ticks per frame, and that tick is justified: a camera is a render-rate concern
// by definition, and every behaviour here is defined against frame delta
// (docs/ARCHITECTURE.md §2.3). All damping is frame-rate independent, so the
// tick rate changes smoothness and never behaviour.
//
// Holds NO tuning values. Everything comes from UVehicleFeelDataAsset.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "GameplayTagContainer.h"
#include "VehicleCameraModeInterface.h"
#include "MidanChaseCameraComponent.generated.h"

class UVehicleFeelDataAsset;
class UMidanVehicleMovementComponent;
class UVehicleSurfaceSensorComponent;

UCLASS(ClassGroup = (Midan), meta = (BlueprintSpawnableComponent))
class MIDANVEHICLE_API UMidanChaseCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	UMidanChaseCameraComponent();

	/**
	 * Cache the feel asset and the components the camera reads from.
	 *
	 * Called by the pawn once the feel asset has loaded. Until then the camera
	 * runs on engine defaults, which looks wrong but is brief and is preferable
	 * to a synchronous load on the game thread.
	 */
	void InitialiseFromAsset(
		const UVehicleFeelDataAsset* InFeel,
		UMidanVehicleMovementComponent* InMovement,
		UVehicleSurfaceSensorComponent* InSurfaceSensor);

	/** Cycle to the next mode. Bound to IA_CameraMode. */
	UFUNCTION(BlueprintCallable, Category = "Midan|Camera")
	void CycleCameraMode();

	UFUNCTION(BlueprintCallable, Category = "Midan|Camera")
	void SetCameraMode(FGameplayTag ModeTag);

	UFUNCTION(BlueprintPure, Category = "Midan|Camera")
	FGameplayTag GetCameraMode() const { return CurrentModeTag; }

	/**
	 * Report a collision impulse. Drives impact shake.
	 *
	 * Pushed by the pawn rather than pulled, because a camera polling for
	 * collisions would either miss them between frames or need its own hit
	 * registration.
	 */
	void ReportImpact(float ImpulseMagnitude);

	/** Steering input, for look-ahead. Pushed each frame by the pawn — the
	 *  camera must not reach back into the input component, or it could
	 *  influence what it observes. */
	void SetSteerInput(float InSteer) { LatestSteerInput = InSteer; }

	/**
	 * Player-facing motion blur multiplier, 0..1.
	 *
	 * ART_DIRECTION §7.3 says never CUT motion blur — it is feel, not fidelity.
	 * But a meaningful minority of players find it unpleasant and some need it
	 * off for motion sensitivity, so the player gets a slider even though the
	 * budget never reclaims it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Midan|Camera")
	void SetMotionBlurScale(float InScale) { MotionBlurScale = FMath::Clamp(InScale, 0.f, 1.f); }

	/** Player-facing camera shake multiplier, 0..1. Accessibility. */
	UFUNCTION(BlueprintCallable, Category = "Midan|Camera")
	void SetShakeScale(float InScale) { ShakeScale = FMath::Clamp(InScale, 0.f, 1.f); }

	/** Hide the camera's own effects for photo mode (ART_DIRECTION §9). */
	UFUNCTION(BlueprintCallable, Category = "Midan|Camera")
	void SetPhotoModeEnabled(bool bEnabled);

	//~ UActorComponent
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Config for the active mode, from the feel asset. */
	const FMidanCameraModeConfig& GetActiveConfig() const;

	/** Boom for chase modes, rigid for bonnet/cockpit. */
	const IMidanVehicleCameraMode& GetActiveMode() const;

	/** Assemble this frame's snapshot from the components. */
	void BuildViewInput(FMidanCameraViewInput& OutInput, float DeltaSeconds) const;

	/** Apply speed-driven post-process. ART_DIRECTION §6 wants chromatic
	 *  aberration, vignette and motion blur all scaled by speed rather than
	 *  fixed — barely perceptible at rest, noticeable at speed. */
	void ApplySpeedPostProcess(float NormalisedSpeed);

	UPROPERTY(Transient)
	TObjectPtr<const UVehicleFeelDataAsset> Feel;

	UPROPERTY(Transient)
	TObjectPtr<UMidanVehicleMovementComponent> Movement;

	UPROPERTY(Transient)
	TObjectPtr<UVehicleSurfaceSensorComponent> SurfaceSensor;

	/** The two behaviours. Stack instances, not allocations — they are stateless
	 *  and all per-frame state lives in ViewState. */
	FMidanBoomCameraMode BoomMode;
	FMidanRigidCameraMode RigidMode;

	FMidanCameraViewState ViewState;

	FGameplayTag CurrentModeTag;

	/** Pending impact impulse, consumed by the next tick. Accumulates as a max
	 *  rather than a sum so several small hits in one frame do not read as one
	 *  large one. */
	float PendingImpactImpulse = 0.f;

	float LatestSteerInput = 0.f;

	float MotionBlurScale = 1.f;
	float ShakeScale = 1.f;

	bool bPhotoMode = false;
	bool bInitialised = false;
};
