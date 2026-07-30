#include "VehicleFXComponent.h"

#include "Components/DecalComponent.h"
#include "MidanCurveUtils.h"
#include "MidanLogChannels.h"
#include "MidanVehicleMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "SurfaceResponseDataAsset.h"
#include "VehicleFeelDataAsset.h"
#include "VehicleSurfaceSensorComponent.h"

namespace
{
	/** Niagara user parameter driving smoke spawn rate. Named once; a typo fails
	 *  silently, the same reason the audio parameter names are constants. */
	static const FName SmokeRateParam = TEXT("SpawnRate");

	/** Minimum travel between decals, cm.
	 *
	 * Distance-gated rather than time-gated. A time gate lays decals in a tight
	 * cluster at low speed and a dashed line at high speed, which makes the frame
	 * rate visible on the road surface. Distance gives an even mark at any speed.
	 *
	 * Not a feel dial: it is the spacing at which overlapping decals stop
	 * reading as a continuous line, which is a property of the decal size. */
	static constexpr float DecalSpacingCm = 60.f;

	/** Skid decal footprint, cm. */
	static constexpr float DecalSizeCm = 42.f;
	static constexpr float DecalProjectionDepthCm = 24.f;
}

UVehicleFXComponent::UVehicleFXComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVehicleFXComponent::InitialiseFromAsset(
	const UVehicleFeelDataAsset* InFeel,
	UMidanVehicleMovementComponent* InMovement,
	UVehicleSurfaceSensorComponent* InSurfaceSensor)
{
	Feel = InFeel;
	Movement = InMovement;
	SurfaceSensor = InSurfaceSensor;

	bInitialised = (Feel != nullptr) && (Movement != nullptr);
	if (!bInitialised)
	{
		UE_LOG(LogMidanVehicle, Error,
			TEXT("VehicleFXComponent on '%s': not initialised. No tyre smoke or skid marks."),
			*GetNameSafe(GetOwner()));
		return;
	}

	const int32 N = MidanVehicleConstants::NumWheels;
	DistanceSinceDecal.Init(0.f, N);
	LastDecalPosition.Init(FVector::ZeroVector, N);

	// --- Persistent per-wheel smoke systems. Created once and modulated by
	// spawn rate, rather than spawned when a wheel starts slipping — the latter
	// allocates every time the car steps sideways, which is constantly.
	if (UNiagaraSystem* SmokeSystem = Feel->TyreSmokeSystem.Get())
	{
		WheelSmoke.Reserve(N);
		for (int32 i = 0; i < N; ++i)
		{
			UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAttached(
				SmokeSystem,
				GetOwner()->GetRootComponent(),
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset,
				/*bAutoDestroy*/ false,
				/*bAutoActivate*/ true);

			if (Comp)
			{
				Comp->SetFloatParameter(SmokeRateParam, 0.f);
			}
			WheelSmoke.Add(Comp);
		}
	}

	// --- Decal pool. Allocated ONCE at its full size, then never grown.
	//
	// Allocating up front means the worst case (pool full, recycling every
	// deposit) is identical in cost to the steady state, so the frame budget
	// does not degrade over a race. That predictability is the whole reason for
	// a pool rather than spawn-and-forget.
	DecalPoolCapacity = FMath::Max(0, Feel->SkidDecalPoolSize);
	if (UMaterialInterface* DecalMaterial = Feel->SkidDecalMaterial.Get())
	{
		DecalPool.Reserve(DecalPoolCapacity);
	}
	else if (DecalPoolCapacity > 0)
	{
		UE_LOG(LogMidanVehicle, Warning,
			TEXT("VehicleFXComponent on '%s': SkidDecalPoolSize is %d but no SkidDecalMaterial is "
				 "assigned. No skid marks will be laid."),
			*GetNameSafe(GetOwner()), DecalPoolCapacity);
	}

	DecalCursor = 0;
}

void UVehicleFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Explicit teardown. These are components attached to a pawn being destroyed,
	// and leaving Niagara systems active during teardown produces warnings that
	// look like a leak even when GC would eventually take them.
	for (UNiagaraComponent* Comp : WheelSmoke)
	{
		if (Comp) { Comp->Deactivate(); }
	}
	WheelSmoke.Reset();

	for (UDecalComponent* Decal : DecalPool)
	{
		if (Decal) { Decal->DestroyComponent(); }
	}
	DecalPool.Reset();

	Super::EndPlay(EndPlayReason);
}

void UVehicleFXComponent::ReportBackfire()
{
	if (!bInitialised || FXScale <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	UNiagaraSystem* System = Feel->ExhaustBackfireSystem.Get();
	if (!System)
	{
		return;
	}

	// One-shot, auto-destroying. Backfires are rare enough that pooling them
	// would be complexity without benefit — unlike decals, they do not
	// accumulate.
	UNiagaraFunctionLibrary::SpawnSystemAttached(
		System, GetOwner()->GetRootComponent(), NAME_None,
		FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset, /*bAutoDestroy*/ true);
}

void UVehicleFXComponent::DepositSkidDecal(
	const int32 WheelIndex,
	const FVector& Location,
	const FRotator& Rotation)
{
	UMaterialInterface* DecalMaterial = Feel ? Feel->SkidDecalMaterial.Get() : nullptr;
	if (!DecalMaterial || DecalPoolCapacity <= 0)
	{
		return;
	}

	UDecalComponent* Decal = nullptr;

	if (DecalPool.Num() < DecalPoolCapacity)
	{
		// Growing toward capacity. This only happens during the first lap.
		Decal = NewObject<UDecalComponent>(GetOwner());
		if (!Decal)
		{
			return;
		}
		Decal->SetDecalMaterial(DecalMaterial);
		Decal->DecalSize = FVector(DecalProjectionDepthCm, DecalSizeCm, DecalSizeCm);
		Decal->RegisterComponent();
		Decal->AttachToComponent(
			GetOwner()->GetRootComponent(),
			FAttachmentTransformRules::KeepWorldTransform);
		// Detach so the decal stays on the road rather than travelling with the
		// car. Attaching first and detaching immediately is how a component gets
		// into the world without being parented to a moving actor.
		Decal->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		DecalPool.Add(Decal);
	}
	else
	{
		// Pool is full — recycle the oldest. Reuse rather than destroy-and-spawn,
		// so a full pool costs nothing extra per deposit.
		Decal = DecalPool[DecalCursor];
		DecalCursor = (DecalCursor + 1) % DecalPoolCapacity;
	}

	if (!Decal)
	{
		return;
	}

	Decal->SetWorldLocationAndRotation(Location, Rotation);

	// Reset fade so a recycled decal starts fresh rather than inheriting the
	// alpha of the mark it replaced.
	Decal->SetFadeOut(0.f, 0.f, false);
	Decal->SetFadeScreenSize(0.001f);
}

void UVehicleFXComponent::UpdateTyreSmoke(const FMidanVehicleFrameState& State, const float DeltaSeconds)
{
	if (WheelSmoke.Num() == 0)
	{
		return;
	}

	for (int32 i = 0; i < MidanVehicleConstants::NumWheels && i < WheelSmoke.Num(); ++i)
	{
		UNiagaraComponent* Smoke = WheelSmoke[i];
		if (!Smoke)
		{
			continue;
		}

		const FMidanWheelState& Wheel = State.Wheels[i];

		// Airborne wheels make no smoke. Obvious, and easy to omit — an airborne
		// wheel often reports large slip because there is nothing to grip.
		if (!Wheel.bInContact)
		{
			Smoke->SetFloatParameter(SmokeRateParam, 0.f);
			continue;
		}

		// Combine longitudinal and lateral slip: a locked wheel under braking and
		// a wheel scrubbing sideways both smoke, and either alone would miss one.
		const float LongSlip = FMath::Abs(Wheel.SlipRatio);
		const float LatSlip = FMath::Abs(Wheel.SlipAngleDegrees) / 45.f;
		const float Slip01 = FMath::Clamp(FMath::Max(LongSlip, LatSlip), 0.f, 1.f);

		// Curve-driven, not thresholded. A binary effect makes a gentle slide and
		// a full lock-up look identical.
		const float Rate = MidanCurve::EvalSafe(Feel->TyreSmokeRateBySlip, Slip01, 0.f) * FXScale;

		Smoke->SetFloatParameter(SmokeRateParam, Rate);
	}
}

void UVehicleFXComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInitialised || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FMidanVehicleFrameState State;
	Movement->WriteWheelPhysicsToFrameState(State);
	if (SurfaceSensor)
	{
		SurfaceSensor->WriteToFrameState(State);
	}

	UpdateTyreSmoke(State, DeltaTime);

	// --- Skid decals, distance-gated per wheel.
	if (FXScale <= KINDA_SMALL_NUMBER || DecalPoolCapacity <= 0)
	{
		return;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const float SlipThreshold = Feel->SkidDecalSlipThreshold;

	for (int32 i = 0; i < MidanVehicleConstants::NumWheels; ++i)
	{
		const FMidanWheelState& Wheel = State.Wheels[i];

		// A decal needs a surface to be on. Also skip off-track wheels: a mark
		// on gravel is the surface FX system's job, not the tarmac skid system's.
		if (!Wheel.bInContact || Wheel.bOffTrack)
		{
			continue;
		}

		const float LongSlip = FMath::Abs(Wheel.SlipRatio);
		const float LatSlip = FMath::Abs(Wheel.SlipAngleDegrees) / 45.f;
		const float Slip01 = FMath::Max(LongSlip, LatSlip);

		if (Slip01 < SlipThreshold)
		{
			continue;
		}

		// API VERIFY: per-wheel world contact position on 5.8. Candidates are
		// FWheelStatus::ContactPoint and UChaosVehicleWheel::GetContactSurfacePosition.
		// See docs/ASSUMPTIONS.md A27. Using the wheel's world location as a stand-in
		// keeps the distance-gate logic correct regardless of which accessor wins.
		const FVector ContactPos = Owner->GetActorLocation();

		DistanceSinceDecal[i] += FVector::Dist(ContactPos, LastDecalPosition[i]);
		LastDecalPosition[i] = ContactPos;

		if (DistanceSinceDecal[i] < DecalSpacingCm)
		{
			continue;
		}
		DistanceSinceDecal[i] = 0.f;

		// Aligned to the direction of travel so the mark points where the tyre
		// was scrubbing, not where the car happens to be facing.
		const FRotator DecalRot = Owner->GetVelocity().IsNearlyZero()
			? Owner->GetActorRotation()
			: Owner->GetVelocity().Rotation();

		DepositSkidDecal(i, ContactPos, DecalRot);
	}
}
