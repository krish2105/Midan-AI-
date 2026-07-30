#include "VehicleAeroComponent.h"

#include "Components/PrimitiveComponent.h"
#include "MidanLogChannels.h"
#include "MidanMathUtils.h"

namespace
{
	/** cm to m. Force is computed and applied in SI; Chaos reports velocity in
	 *  cm/s and offsets in cm, so the conversion appears in every force term.
	 *  Named because a bare 0.01f repeated five times is one typo away from a
	 *  force that is out by 100x and looks like a physics bug. */
	static constexpr float CmToM = 0.01f;
}

UVehicleAeroComponent::UVehicleAeroComponent()
{
	// No game-thread tick. All work happens in the async physics callback.
	PrimaryComponentTick.bCanEverTick = false;
}

void UVehicleAeroComponent::BeginPlay()
{
	Super::BeginPlay();

	// API VERIFY: SetAsyncPhysicsTickEnabled is the UE5 opt-in for
	// AsyncPhysicsTickComponent. Confirm the exact name and that it must be
	// called after BeginPlay rather than in the constructor — see
	// docs/ASSUMPTIONS.md A21.
	SetAsyncPhysicsTickEnabled(true);
}

void UVehicleAeroComponent::InitialiseFromConfig(
	const FVehicleAeroConfig& InConfig,
	UPrimitiveComponent* InTargetBody,
	const float VehicleMassKg)
{
	CachedConfig = InConfig;
	TargetBody = InTargetBody;
	CachedMassKg = VehicleMassKg;

	// Precompute everything invariant. The callback runs 120 times a second per
	// vehicle and up to eight vehicles exist, so 960 evaluations per second —
	// each saved multiply is worth having, and more importantly each value not
	// recomputed is one fewer chance of a per-substep unit-conversion mistake.
	//
	// Force is computed in SI (Newtons) and applied in SI. Velocity arrives in
	// cm/s from Chaos, so the v² term converts to m/s first: (v_cm/100)^2.
	const float DynamicPressureFactor = 0.5f * MidanMath::AirDensitySeaLevel * CachedConfig.FrontalAreaM2;

	DragConstant            = DynamicPressureFactor * CachedConfig.DragCoefficient;
	FrontDownforceConstant  = DynamicPressureFactor * CachedConfig.FrontLiftCoefficient;
	RearDownforceConstant   = DynamicPressureFactor * CachedConfig.RearLiftCoefficient;

	MinSpeedForDownforceCmS = CachedConfig.MinSpeedForDownforceKmh * MidanMath::KmHToCmS;

	// Safety clamp in absolute Newtons. Not a tuning dial — it stops a
	// mis-authored coefficient from pinning the car to the ground or feeding the
	// solver a force large enough to destabilise it.
	MaxTotalDownforceN = CachedConfig.MaxDownforceAsWeightMultiple * CachedMassKg * (MidanMath::GravityCmS2 * CmToM);

	bInitialised = (TargetBody != nullptr) && (CachedMassKg > KINDA_SMALL_NUMBER);

	if (!bInitialised)
	{
		// Log at init, never in the callback. A car with no aero is a real
		// handling difference, so this must not fail silently.
		UE_LOG(LogMidanVehicle, Error,
			TEXT("VehicleAeroComponent on '%s': not initialised (body %s, mass %.1fkg). "
				 "No aero forces will be applied."),
			*GetNameSafe(GetOwner()),
			TargetBody ? TEXT("ok") : TEXT("NULL"),
			CachedMassKg);
	}
}

float UVehicleAeroComponent::GetLastDownforceAsWeightMultiple() const
{
	const float WeightN = CachedMassKg * (MidanMath::GravityCmS2 * CmToM);
	return MidanMath::SafeDivide(LastFrontDownforceN + LastRearDownforceN, WeightN, 0.f);
}

void UVehicleAeroComponent::ComputeAeroForces(
	const FVehicleAeroConfig& Config,
	const float VehicleMassKg,
	const FVector& VelocityCmS,
	const FVector& ForwardDir,
	const FVector& UpDir,
	FVector& OutDragForce,
	FVector& OutFrontDownforce,
	FVector& OutRearDownforce)
{
	OutDragForce = FVector::ZeroVector;
	OutFrontDownforce = FVector::ZeroVector;
	OutRearDownforce = FVector::ZeroVector;

	const float SpeedCmS = VelocityCmS.Size();
	if (SpeedCmS < KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Convert to m/s for SI force output.
	const float SpeedMS = SpeedCmS * CmToM;
	const float SpeedSquared = SpeedMS * SpeedMS;

	const float DynamicPressureFactor = 0.5f * MidanMath::AirDensitySeaLevel * Config.FrontalAreaM2;

	// --- Drag: -0.5 * rho * Cd * A * v^2 * v_hat
	// Opposes the actual velocity direction, not the forward vector. Those differ
	// in a slide, and using forward would produce a drag force that pushes the
	// car sideways — subtly wrong and very hard to spot.
	const FVector VelocityDir = VelocityCmS / SpeedCmS;
	const float DragMagnitudeN = DynamicPressureFactor * Config.DragCoefficient * SpeedSquared;
	OutDragForce = -VelocityDir * DragMagnitudeN;

	// --- Downforce: -0.5 * rho * Cl * A * v^2, applied along the chassis DOWN
	// axis, split front and rear.
	const float SpeedThresholdCmS = Config.MinSpeedForDownforceKmh * MidanMath::KmHToCmS;
	if (SpeedCmS < SpeedThresholdCmS)
	{
		// Below the threshold v² is negligible and skipping avoids pointless
		// work. This is also why the hypercar is "nervous below 80km/h" — with no
		// aero, the mechanical balance shows through.
		return;
	}

	// Longitudinal speed drives downforce, not total speed. A car sliding
	// sideways at 100km/h is not generating 100km/h of downforce — the wing is
	// stalled. Using total speed would make a drift artificially stable, which
	// removes the risk that makes drifting interesting.
	const float ForwardSpeedMS = FMath::Abs(FVector::DotProduct(VelocityCmS, ForwardDir)) * CmToM;
	const float ForwardSpeedSquared = ForwardSpeedMS * ForwardSpeedMS;

	float FrontN = DynamicPressureFactor * Config.FrontLiftCoefficient * ForwardSpeedSquared;
	float RearN  = DynamicPressureFactor * Config.RearLiftCoefficient  * ForwardSpeedSquared;

	// Clamp total, preserving the front/rear ratio. Clamping each independently
	// would silently shift the aero balance at high speed, which is precisely
	// the property being tuned.
	const float WeightN = VehicleMassKg * (MidanMath::GravityCmS2 * CmToM);
	const float MaxTotalN = Config.MaxDownforceAsWeightMultiple * WeightN;
	const float TotalN = FrontN + RearN;
	if (TotalN > MaxTotalN && TotalN > KINDA_SMALL_NUMBER)
	{
		const float Scale = MaxTotalN / TotalN;
		FrontN *= Scale;
		RearN  *= Scale;
	}

	OutFrontDownforce = -UpDir * FrontN;
	OutRearDownforce  = -UpDir * RearN;
}

void UVehicleAeroComponent::AsyncPhysicsTickComponent(const float DeltaTime, const float SimTime)
{
	Super::AsyncPhysicsTickComponent(DeltaTime, SimTime);

	// Cheapest possible early-out first. An uninitialised or disabled component
	// must cost essentially nothing, because it is still being called 120 times
	// a second.
	if (!bInitialised || !bRuntimeEnabled || !CachedConfig.bEnabled)
	{
		return;
	}

	UPrimitiveComponent* Body = TargetBody.Get();
	if (!Body)
	{
		return;
	}

	// --- Read state. All stack locals, no allocation.
	const FTransform BodyTransform = Body->GetComponentTransform();
	const FVector VelocityCmS = Body->GetPhysicsLinearVelocity();
	const FVector ForwardDir = BodyTransform.GetUnitAxis(EAxis::X);
	const FVector UpDir = BodyTransform.GetUnitAxis(EAxis::Z);

	FVector DragForce, FrontDownforce, RearDownforce;
	ComputeAeroForces(
		CachedConfig, CachedMassKg, VelocityCmS, ForwardDir, UpDir,
		DragForce, FrontDownforce, RearDownforce);

	// --- Apply. Three AddForceAtLocation calls, at the three authored points.
	//
	// The separate front and rear application points are the entire reason this
	// component exists. Applying total downforce at the centre of mass would
	// produce the same total grip and none of the balance behaviour.
	//
	// API VERIFY: AddForceAtLocation with bAccelChange=false expects force in
	// Newtons and a world-space location. Confirm against 5.8 —
	// docs/ASSUMPTIONS.md A21.
	if (!DragForce.IsNearlyZero())
	{
		const FVector DragPoint = BodyTransform.TransformPosition(CachedConfig.CentreOfPressureOffset);
		Body->AddForceAtLocation(DragForce, DragPoint);
	}

	if (!FrontDownforce.IsNearlyZero())
	{
		const FVector FrontPoint = BodyTransform.TransformPosition(CachedConfig.FrontApplicationOffset);
		Body->AddForceAtLocation(FrontDownforce, FrontPoint);
	}

	if (!RearDownforce.IsNearlyZero())
	{
		const FVector RearPoint = BodyTransform.TransformPosition(CachedConfig.RearApplicationOffset);
		Body->AddForceAtLocation(RearDownforce, RearPoint);
	}

	// --- Record magnitudes for debug readout. Three float writes; no formatting,
	// no logging, no container. Debug DRAWING happens on the game thread from
	// these values, never here — DrawDebugLine from a physics callback is both a
	// thread-safety violation and an allocation.
	LastDragMagnitudeN = DragForce.Size();
	LastFrontDownforceN = FrontDownforce.Size();
	LastRearDownforceN = RearDownforce.Size();
}
