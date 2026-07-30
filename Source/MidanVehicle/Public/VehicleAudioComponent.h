// Engine, tyre, wind and impact audio. Parameter mapping only.
//
// Responsibility: push simulation state into MetaSound parameters.
// Single reason to change: the parameter contract with the sound design changes.
//
// THE MAPPING IS ENGINEERING; THE SAMPLE CONTENT IS TASTE. This component
// decides what "engine load" means numerically and pushes it; the MetaSound
// graph decides what that sounds like. Mixing the two makes both harder to
// change — and means a sound designer cannot iterate without a programmer.
//
// TWO-AXIS BLEND. RPM on one axis, engine LOAD on the other, with separate
// on-load and off-load sample sets in the graph. Single-axis RPM blending is the
// classic mistake and produces a whine; two axes produce an engine. Master
// prompt §2.2 requires a minimum of four RPM layers, and the load axis is what
// makes lifting off the throttle mean something.
//
// Driven from SIMULATION STATE, never from animation events. Slip-driven tyre
// scrub responds to the actual physics; an animation-triggered skid sound does
// not, and the difference is audible the first time a player catches a slide.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleAudioComponent.generated.h"

class UAudioComponent;
class UMidanVehicleMovementComponent;
class USoundBase;
class UVehicleFeelDataAsset;
class UVehicleSurfaceSensorComponent;

/**
 * MetaSound parameter names, declared once.
 *
 * These are the contract with the sound design. A typo here fails silently —
 * MetaSound ignores an unknown parameter name — so they are named constants
 * rather than string literals at call sites, for exactly the reason CLAUDE.md
 * bans string-constructed gameplay tags.
 */
namespace MidanAudioParams
{
	/** Normalised RPM, 0..1 across the usable band. Primary blend axis. */
	static const FName RPM = TEXT("RPM");

	/** Engine load, 0..1. The SECOND axis, and the one usually missing.
	 *  Crossfades the on-load and off-load sample sets. */
	static const FName Load = TEXT("Load");

	/** Normalised speed, 0..1. Drives wind and any speed-dependent filtering. */
	static const FName Speed = TEXT("Speed");

	/** Current gear, for shift-aware behaviour in the graph. */
	static const FName Gear = TEXT("Gear");

	/** True while a shift is in progress, so the graph can duck or interrupt. */
	static const FName Shifting = TEXT("Shifting");

	/** Front and rear scrub separately, so oversteer and understeer sound
	 *  DIFFERENT. A single combined slip value makes both sound identical,
	 *  which throws away the clearest audio cue the player has. */
	static const FName SlipFront = TEXT("SlipFront");
	static const FName SlipRear = TEXT("SlipRear");

	/** Surface roughness under the wheels, 0..1. */
	static const FName SurfaceRoughness = TEXT("SurfaceRoughness");
}

UCLASS(ClassGroup = (Midan), meta = (BlueprintSpawnableComponent))
class MIDANVEHICLE_API UVehicleAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleAudioComponent();

	/** Feel asset must already be loaded — the pawn resolves the soft pointers. */
	void InitialiseFromAsset(
		const UVehicleFeelDataAsset* InFeel,
		UMidanVehicleMovementComponent* InMovement,
		UVehicleSurfaceSensorComponent* InSurfaceSensor,
		float InMaxRPM);

	/** Fire a one-shot impact. Pushed by the pawn on collision, same source as
	 *  the camera shake, so the two channels agree about what just happened. */
	void ReportImpact(float ImpulseMagnitude);

	/** Fire a backfire one-shot. Triggered on an upshift under load, and on
	 *  throttle lift at high RPM. */
	void ReportBackfire();

	/** Master multiplier for every vehicle sound. Used to duck the player's own
	 *  car during results and the countdown. */
	UFUNCTION(BlueprintCallable, Category = "Midan|Audio")
	void SetVehicleAudioScale(float InScale);

	//~ UActorComponent
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Compute engine load, 0..1.
	 *
	 * Load is NOT throttle. It is how hard the engine is working: throttle
	 * weighted by how much resistance it is meeting. At full throttle in top
	 * gear on a straight, load is high; at full throttle with the wheels
	 * spinning, load is low, because the engine is meeting nothing. That
	 * distinction is what makes wheelspin audible. */
	float ComputeEngineLoad() const;

	/** Spawn a persistent looping component for a sound, or null if unset. */
	UAudioComponent* SpawnLoop(const TSoftObjectPtr<USoundBase>& Sound, FName DebugName) const;

	UPROPERTY(Transient)
	TObjectPtr<const UVehicleFeelDataAsset> Feel;

	UPROPERTY(Transient)
	TObjectPtr<UMidanVehicleMovementComponent> Movement;

	UPROPERTY(Transient)
	TObjectPtr<UVehicleSurfaceSensorComponent> SurfaceSensor;

	//~ Persistent loops. Created once, parameters pushed per frame. Creating and
	//  destroying an audio component per frame would be both an allocation and
	//  an audible restart.
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> EngineLoop;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> TyreLoop;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WindLoop;

	float MaxRPM = 8000.f;
	float AudioScale = 1.f;

	/** Smoothed load, so the second blend axis does not chatter. Raw load is
	 *  noisy at the substep level and an unsmoothed crossfade sounds like a
	 *  fault. */
	float SmoothedLoad = 0.f;

	/** Previous gear, for shift detection. */
	int32 PreviousGear = 0;

	/** Seconds remaining on the shift flag. */
	float ShiftFlagTimer = 0.f;

	/** Minimum seconds between backfires. Without a floor, a series of upshifts
	 *  produces a machine-gun stutter that reads as a bug. */
	float BackfireCooldown = 0.f;

	bool bInitialised = false;
};
