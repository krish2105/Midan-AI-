// Drag and downforce, applied in the async physics callback.
//
// Responsibility: aerodynamic force application.
// Single reason to change: the aero force model changes.
//
// Chaos does not provide usable downforce, so we apply it ourselves. The value
// of doing it by hand is the SEPARATE front and rear application points: front
// and rear downforce balance shifting with speed is what makes a car feel
// planted at 250km/h and nervous at 60. That is a speed-dependent handling
// personality from four numbers, and no amount of suspension tuning imitates it.
//
// ZERO ALLOCATION. This runs inside the async physics callback at 120Hz, where
// CLAUDE.md forbids any heap allocation. Every value it needs is either a cached
// POD member or read from an inline FRuntimeFloatCurve. No TArray, no FString,
// no NewObject, no logging in the steady-state path.
//
// Runs in the callback rather than Tick because force applied at frame rate
// against physics running at 120Hz produces frame-rate-dependent handling — the
// car behaves differently at 30fps and 144fps, which is a bug that hides behind
// frame-rate dependence and is miserable to find.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Config/VehicleAeroConfig.h"
#include "VehicleAeroComponent.generated.h"

class UPrimitiveComponent;

UCLASS(ClassGroup = (Midan), meta = (BlueprintSpawnableComponent))
class MIDANVEHICLE_API UVehicleAeroComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleAeroComponent();

	/**
	 * Cache the config and the target body. Called by the pawn after the setup
	 * asset has been applied.
	 *
	 * The config is COPIED, not referenced. The callback must not chase a pointer
	 * into a UObject that could be reloaded by the editor hot-reload delegate
	 * mid-substep, and a by-value POD copy removes that entire class of problem.
	 */
	void InitialiseFromConfig(const FVehicleAeroConfig& InConfig, UPrimitiveComponent* InTargetBody, float VehicleMassKg);

	/** Stop applying force. Used when the vehicle is respawning or the race has
	 *  ended, so a parked car is not still being pressed into the ground. */
	void SetAeroEnabled(bool bEnabled) { bRuntimeEnabled = bEnabled; }
	bool IsAeroEnabled() const { return bRuntimeEnabled && CachedConfig.bEnabled; }

	//~ Debug readouts. Written every callback, read by the debug draw and the
	//  Phase 3 gate report. Plain floats so reading them costs nothing.
	float GetLastDragForceN() const { return LastDragMagnitudeN; }
	float GetLastFrontDownforceN() const { return LastFrontDownforceN; }
	float GetLastRearDownforceN() const { return LastRearDownforceN; }

	/** Total downforce as a multiple of vehicle weight. The number to watch when
	 *  tuning: above about 2.0 the car is on rails and the handling model stops
	 *  being interesting. */
	float GetLastDownforceAsWeightMultiple() const;

	//~ UActorComponent
	virtual void BeginPlay() override;

	/**
	 * The physics callback. Everything this component exists to do happens here.
	 *
	 * API VERIFY: UActorComponent::AsyncPhysicsTickComponent is the UE5 async
	 * physics hook, enabled via SetAsyncPhysicsTickEnabled. Signature and
	 * override-ness need confirming against 5.8 headers at first compile —
	 * docs/ASSUMPTIONS.md A21 covers this class of check.
	 */
	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;

private:
	/**
	 * Compute drag and downforce for a given velocity.
	 *
	 * Static and free of engine state so the force model is unit-testable
	 * without a world — the Phase 3 gate needs to show the v² relationship and
	 * the weight clamp behave correctly, and that should not require spawning a
	 * car. Pure math per CLAUDE.md.
	 */
	static void ComputeAeroForces(
		const FVehicleAeroConfig& Config,
		float VehicleMassKg,
		const FVector& VelocityCmS,
		const FVector& ForwardDir,
		const FVector& UpDir,
		FVector& OutDragForce,
		FVector& OutFrontDownforce,
		FVector& OutRearDownforce);

	/** Copied by value at init — see InitialiseFromConfig. */
	FVehicleAeroConfig CachedConfig;

	/** Weak by intent: the body is owned by the pawn's mesh and this component
	 *  must not keep it alive. Raw because TObjectPtr access in a physics
	 *  callback is not worth the access-tracking overhead, and the pointer is
	 *  cleared in EndPlay. */
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> TargetBody;

	float CachedMassKg = 0.f;

	/** Precomputed 0.5 * rho * Cd * A, so the callback does one multiply by v²
	 *  instead of four multiplies every substep. */
	float DragConstant = 0.f;
	float FrontDownforceConstant = 0.f;
	float RearDownforceConstant = 0.f;

	/** Precomputed cm/s threshold, so the callback compares against cm/s
	 *  directly rather than converting to km/h every substep. */
	float MinSpeedForDownforceCmS = 0.f;

	/** Precomputed absolute clamp in Newtons. */
	float MaxTotalDownforceN = 0.f;

	bool bRuntimeEnabled = true;
	bool bInitialised = false;

	//~ Debug state.
	float LastDragMagnitudeN = 0.f;
	float LastFrontDownforceN = 0.f;
	float LastRearDownforceN = 0.f;
};
