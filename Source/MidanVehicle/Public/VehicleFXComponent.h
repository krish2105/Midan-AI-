// Tyre smoke, surface particles, skid decals and backfire.
//
// Responsibility: visual feedback scaled by simulation state.
// Single reason to change: a visual feedback channel is added.
//
// THE DECAL POOL IS THE POINT OF THIS FILE. Decals deposited over a session are
// a slow leak that only shows up on lap four — by which time the frame is gone
// and the cause looks like "the track got slower", not "we leaked decals". A
// fixed-size ring with the oldest recycled makes the worst case identical to the
// steady state, which is the only version you can budget for.
//
// Everything here is magnitude-scaled from a curve, never an on/off trigger. A
// binary smoke effect makes a gentle slide and a full lock-up look identical,
// which throws away the information the effect exists to convey.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MidanCoreTypes.h"
#include "VehicleFXComponent.generated.h"

class UMidanVehicleMovementComponent;
class UNiagaraComponent;
class UVehicleFeelDataAsset;
class UVehicleSurfaceSensorComponent;
class UDecalComponent;

UCLASS(ClassGroup = (Midan), meta = (BlueprintSpawnableComponent))
class MIDANVEHICLE_API UVehicleFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleFXComponent();

	void InitialiseFromAsset(
		const UVehicleFeelDataAsset* InFeel,
		UMidanVehicleMovementComponent* InMovement,
		UVehicleSurfaceSensorComponent* InSurfaceSensor);

	/** Spawn a backfire burst. Same trigger as the audio backfire, so the two
	 *  channels fire together rather than drifting apart. */
	void ReportBackfire();

	/** Player-facing FX intensity, 0..1. Zero disables particle spawning and
	 *  decal deposition entirely — a genuine performance lever as well as an
	 *  accessibility one. */
	UFUNCTION(BlueprintCallable, Category = "Midan|FX")
	void SetFXScale(float InScale) { FXScale = FMath::Clamp(InScale, 0.f, 1.f); }

	/** Current decals in the pool. Exposed for the Phase 8 profiling report —
	 *  "the pool is full and recycling" is the state you want to confirm under
	 *  load, not assume. */
	UFUNCTION(BlueprintPure, Category = "Midan|FX")
	int32 GetActiveDecalCount() const { return DecalPool.Num(); }

	//~ UActorComponent
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Update per-wheel smoke rate from slip. */
	void UpdateTyreSmoke(const FMidanVehicleFrameState& State, float DeltaSeconds);

	/**
	 * Deposit a skid decal under a wheel, recycling the oldest when the pool is
	 * full.
	 *
	 * The pool is allocated ONCE at init and never grows. Recycling reuses the
	 * existing component rather than destroying and spawning, so a full pool
	 * costs the same as an empty one every frame after the first lap.
	 */
	void DepositSkidDecal(int32 WheelIndex, const FVector& Location, const FRotator& Rotation);

	UPROPERTY(Transient)
	TObjectPtr<const UVehicleFeelDataAsset> Feel;

	UPROPERTY(Transient)
	TObjectPtr<UMidanVehicleMovementComponent> Movement;

	UPROPERTY(Transient)
	TObjectPtr<UVehicleSurfaceSensorComponent> SurfaceSensor;

	/** One persistent smoke system per wheel. Created once; the spawn RATE is
	 *  modulated per frame rather than the system being spawned and destroyed,
	 *  which would allocate every time a wheel started slipping. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> WheelSmoke;

	/** Fixed-size ring. Never grows past SkidDecalPoolSize. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> DecalPool;

	/** Next index to recycle. Wraps, giving oldest-first replacement without
	 *  needing timestamps or a sort. */
	int32 DecalCursor = 0;

	int32 DecalPoolCapacity = 0;

	/** Distance travelled since the last decal was laid, per wheel, cm.
	 *  Deposition is DISTANCE-gated, not time-gated: a time gate lays decals in
	 *  a tight cluster at low speed and a dashed line at high speed, which reads
	 *  as the frame rate being visible. */
	TArray<float> DistanceSinceDecal;

	/** Last wheel contact position, for the distance gate. */
	TArray<FVector> LastDecalPosition;

	float FXScale = 1.f;
	bool bInitialised = false;
};
