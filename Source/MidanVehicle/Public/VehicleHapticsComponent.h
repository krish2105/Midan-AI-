// Force feedback. The second output channel most projects leave silent.
//
// Responsibility: drive controller haptics from simulation state.
// Single reason to change: a haptic channel is added.
//
// Underused and cheap. Two kinds of effect, and mixing them up is the usual
// mistake:
//
//   CONTINUOUS — engine idle, surface texture, wheel slip. State, not events.
//                Amplitude modulated per frame.
//   TRANSIENT  — impacts and kerb strikes. Events, fired once.
//
// Amplitude comes from the SAME curve family as the camera shake
// (HapticAmplitudeByImpulse alongside ImpactShakeByImpulse), so the visual,
// audio and haptic channels agree about how big an event was. Three channels
// disagreeing about the same collision is worse than any one of them being
// absent.
//
// Intensity slider and an off switch are mandatory: accessibility, and a
// meaningful number of players simply dislike rumble.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleHapticsComponent.generated.h"

class APlayerController;
class UForceFeedbackComponent;
class UMidanVehicleMovementComponent;
class UVehicleFeelDataAsset;
class UVehicleSurfaceSensorComponent;

UCLASS(ClassGroup = (Midan), meta = (BlueprintSpawnableComponent))
class MIDANVEHICLE_API UVehicleHapticsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleHapticsComponent();

	void InitialiseFromAsset(
		const UVehicleFeelDataAsset* InFeel,
		UMidanVehicleMovementComponent* InMovement,
		UVehicleSurfaceSensorComponent* InSurfaceSensor);

	/** Fire a transient impact. Same reported impulse the camera and audio use. */
	void ReportImpact(float ImpulseMagnitude);

	/** Fire a kerb strike. Distinct from a general impact because a kerb is a
	 *  deliberate part of driving and should feel like texture rather than
	 *  damage. */
	void ReportKerbStrike(float Intensity);

	/**
	 * Player-facing intensity, 0..1. Zero disables haptics entirely.
	 *
	 * Mandatory, not optional. Some players find sustained rumble physically
	 * unpleasant and some controllers make it far stronger than intended.
	 */
	UFUNCTION(BlueprintCallable, Category = "Midan|Haptics")
	void SetHapticIntensity(float InIntensity);

	UFUNCTION(BlueprintPure, Category = "Midan|Haptics")
	float GetHapticIntensity() const { return HapticIntensity; }

	//~ UActorComponent
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Owning player controller, or null for an AI vehicle. An opponent has no
	 *  controller to rumble, and that is the normal case for seven of the eight
	 *  cars — so a null here is not an error. */
	APlayerController* GetOwningPlayerController() const;

	/** Play a one-shot force feedback effect scaled by intensity. */
	void PlayTransient(const TSoftObjectPtr<class UForceFeedbackEffect>& Effect, float Scale);

	UPROPERTY(Transient)
	TObjectPtr<const UVehicleFeelDataAsset> Feel;

	UPROPERTY(Transient)
	TObjectPtr<UMidanVehicleMovementComponent> Movement;

	UPROPERTY(Transient)
	TObjectPtr<UVehicleSurfaceSensorComponent> SurfaceSensor;

	/** Continuous effects, kept alive and modulated rather than retriggered.
	 *  Retriggering a looping effect every frame produces a stutter, not a
	 *  rumble. */
	UPROPERTY(Transient)
	TObjectPtr<UForceFeedbackComponent> IdleRumble;

	UPROPERTY(Transient)
	TObjectPtr<UForceFeedbackComponent> SurfaceRumble;

	UPROPERTY(Transient)
	TObjectPtr<UForceFeedbackComponent> SlipRumble;

	float HapticIntensity = 1.f;

	/** Smoothed continuous amplitudes. Raw per-frame values chatter, and a
	 *  chattering rumble reads as a fault in the controller. */
	float SmoothedSurfaceAmplitude = 0.f;
	float SmoothedSlipAmplitude = 0.f;

	/** Seconds until another kerb strike may fire. Kerbs are struck in rapid
	 *  succession by design, and without a floor the transients overlap into
	 *  continuous buzz. */
	float KerbCooldown = 0.f;

	bool bInitialised = false;
};
