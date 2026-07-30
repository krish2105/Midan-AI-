// Per-wheel surface detection from physical materials.
//
// Responsibility: determine what each wheel is touching, and its response row.
// Single reason to change: how surface state is sampled changes.
//
// PHYSICAL MATERIALS, NEVER TRIGGER VOLUMES. Master prompt §1.2 is explicit and
// the reasoning is worth restating: trigger volumes duplicate the track shape,
// go stale the first time a corner moves, and cannot express "two wheels on
// gravel" — which is exactly the state that makes the rally car interesting.
// Per-wheel sampling is also what lets the car pull when the right-hand wheels
// drop onto gravel and the left stay on tarmac.
//
// ZERO ALLOCATION. Runs in the async physics callback. The per-wheel results
// live in a fixed-size array sized by MidanVehicleConstants::NumWheels, and the
// response rows are cached as raw pointers into the surface asset's array at
// init — so a lookup in the callback is an array index, not a search.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MidanCoreTypes.h"
#include "Chaos/ChaosEngineInterface.h"
#include "VehicleSurfaceSensorComponent.generated.h"

class UChaosWheeledVehicleMovementComponent;
class USurfaceResponseDataAsset;
struct FSurfaceResponseRow;

UCLASS(ClassGroup = (Midan), meta = (BlueprintSpawnableComponent))
class MIDANVEHICLE_API UVehicleSurfaceSensorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleSurfaceSensorComponent();

	/**
	 * Cache the movement component and the surface table.
	 *
	 * The surface asset must already be loaded — the pawn resolves the soft
	 * pointer, because CLAUDE.md forbids a synchronous load and a physics
	 * callback obviously cannot wait for one.
	 */
	void InitialiseFromAsset(UChaosWheeledVehicleMovementComponent* InMovement, const USurfaceResponseDataAsset* InSurfaceAsset);

	//~ Per-wheel readouts. Const and cheap; safe from the game thread between
	//  substeps. Index with MidanVehicleConstants::WheelFrontLeft etc.

	/** Surface tag under a wheel. Empty tag when airborne — an empty tag means
	 *  "unknown", NOT "tarmac", and callers must not default it. */
	FGameplayTag GetWheelSurfaceTag(int32 WheelIndex) const;

	/** Response row under a wheel, or null when airborne or unmatched. */
	const FSurfaceResponseRow* GetWheelSurfaceRow(int32 WheelIndex) const;

	/** Grip multiplier under a wheel. Returns 1.0 when airborne so a caller
	 *  multiplying by it is not surprised by a zero. */
	float GetWheelFrictionMultiplier(int32 WheelIndex) const;

	UFUNCTION(BlueprintPure, Category = "Midan|Surface")
	bool IsWheelOffTrack(int32 WheelIndex) const;

	/** How many wheels are off the racing surface. Phase 5's lap invalidation
	 *  thresholds on this with a grace time, rather than on any single wheel —
	 *  so clipping a kerb does not cost a lap. */
	UFUNCTION(BlueprintPure, Category = "Midan|Surface")
	int32 GetOffTrackWheelCount() const;

	/** Mean roughness across grounded wheels, 0..1. Drives camera rumble and
	 *  haptics. Zero when fully airborne, which is correct — air is smooth. */
	UFUNCTION(BlueprintPure, Category = "Midan|Surface")
	float GetAverageRoughness() const;

	/** Copy sampled state into a frame state's wheel array. Called by the pawn
	 *  when assembling telemetry, so the sensor owns the surface fields and the
	 *  pawn owns the rest. */
	void WriteToFrameState(FMidanVehicleFrameState& OutState) const;

	//~ UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;

private:
	/** One wheel's sampled surface. Kept minimal — this array is written every
	 *  substep. */
	struct FWheelSurfaceSample
	{
		/** Raw pointer into the surface asset's Surfaces array. Valid for as long
		 *  as CachedSurfaceAsset is, which the UPROPERTY guarantees. Cached so a
		 *  callback lookup is a dereference rather than a linear search. */
		const FSurfaceResponseRow* Row = nullptr;

		FGameplayTag SurfaceTag;
		bool bInContact = false;
		bool bOffTrack = false;
		float SuspensionCompression = 0.f;
	};

	FWheelSurfaceSample Samples[MidanVehicleConstants::NumWheels];

	UPROPERTY(Transient)
	TObjectPtr<UChaosWheeledVehicleMovementComponent> Movement;

	UPROPERTY(Transient)
	TObjectPtr<const USurfaceResponseDataAsset> CachedSurfaceAsset;

	/**
	 * Direct lookup from EPhysicalSurface to a response row.
	 *
	 * Built once at init. EPhysicalSurface has a fixed maximum, so a flat array
	 * indexed by the enum turns the per-wheel-per-substep lookup into one array
	 * index — 4 wheels x 120Hz x 8 vehicles is 3840 lookups per second, which is
	 * enough that a linear search through the asset would show up in a profile.
	 */
	static constexpr int32 MaxPhysicalSurfaces = SurfaceType_Max;
	const FSurfaceResponseRow* SurfaceLookup[MaxPhysicalSurfaces] = {};

	/** Row used when a wheel reports a surface with no authored row. Prevents a
	 *  missing row from reading as "no grip". */
	const FSurfaceResponseRow* DefaultRow = nullptr;

	bool bInitialised = false;
};
