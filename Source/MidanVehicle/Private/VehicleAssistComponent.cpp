#include "VehicleAssistComponent.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "MidanLogChannels.h"
#include "MidanMathUtils.h"

UVehicleAssistComponent::UVehicleAssistComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVehicleAssistComponent::InitialiseFromConfig(
	const FVehicleAssistConfig& InConfig,
	UChaosWheeledVehicleMovementComponent* InMovement)
{
	CachedConfig = InConfig;
	Movement = InMovement;

	// Seed the runtime mask from the authored defaults. The settings menu can
	// override afterwards; the asset supplies what the car ships with.
	EnabledMask = EMidanAssistFlags::None;
	if (CachedConfig.bTractionControlEnabled)  { EnumAddFlags(EnabledMask, EMidanAssistFlags::TractionControl); }
	if (CachedConfig.bABSEnabled)              { EnumAddFlags(EnabledMask, EMidanAssistFlags::ABS); }
	if (CachedConfig.bStabilityControlEnabled) { EnumAddFlags(EnabledMask, EMidanAssistFlags::Stability); }
	if (CachedConfig.bSteeringAssistEnabled)   { EnumAddFlags(EnabledMask, EMidanAssistFlags::SteeringAssist); }

	bInitialised = (Movement != nullptr);

	if (!bInitialised)
	{
		UE_LOG(LogMidanVehicle, Error,
			TEXT("VehicleAssistComponent on '%s': no movement component. No assist will function."),
			*GetNameSafe(GetOwner()));
	}
}

void UVehicleAssistComponent::SetAssistEnabled(const EMidanAssistFlags Assist, const bool bEnabled)
{
	if (bEnabled)
	{
		EnumAddFlags(EnabledMask, Assist);
	}
	else
	{
		EnumRemoveFlags(EnabledMask, Assist);
	}
}

bool UVehicleAssistComponent::IsAssistEnabled(const EMidanAssistFlags Assist) const
{
	return EnumHasAllFlags(EnabledMask, Assist);
}

float UVehicleAssistComponent::ComputeTractionControlMultiplier() const
{
	UChaosWheeledVehicleMovementComponent* Move = Movement.Get();
	if (!Move)
	{
		return 1.f;
	}

	// Worst (highest) positive slip across driven wheels governs. Averaging would
	// let one spinning wheel hide behind three gripping ones — exactly the case
	// traction control exists to catch.
	float WorstSlip = 0.f;
	const int32 WheelCount = FMath::Min(Move->Wheels.Num(), MidanVehicleConstants::NumWheels);

	for (int32 i = 0; i < WheelCount; ++i)
	{
		// API VERIFY: per-wheel slip ratio accessor on 5.8 — see
		// docs/ASSUMPTIONS.md A21. Logic is accessor-independent.
		const FWheelStatus& Status = Move->GetWheelState(i);
		if (Status.bInContact)
		{
			WorstSlip = FMath::Max(WorstSlip, Status.LongitudinalSlip);
		}
	}

	if (WorstSlip <= CachedConfig.TractionControlSlipThreshold)
	{
		return 1.f;
	}

	// Proportional cut above the threshold, scaled by strength. Strength below
	// 1.0 means TC never fully closes the throttle, which is deliberate: a full
	// cut feels like the engine died, and a partial cut feels like grip.
	const float Excess = WorstSlip - CachedConfig.TractionControlSlipThreshold;
	const float Normalised = FMath::Clamp(Excess / FMath::Max(CachedConfig.TractionControlSlipThreshold, KINDA_SMALL_NUMBER), 0.f, 1.f);
	return FMath::Clamp(1.f - (Normalised * CachedConfig.TractionControlStrength), 0.f, 1.f);
}

float UVehicleAssistComponent::ComputeABSMultiplier() const
{
	UChaosWheeledVehicleMovementComponent* Move = Movement.Get();
	if (!Move)
	{
		return 1.f;
	}

	// Most negative slip governs. A single locked wheel flat-spots the tyre and
	// costs steering authority, so the worst wheel decides for all of them.
	float WorstLock = 0.f;
	const int32 WheelCount = FMath::Min(Move->Wheels.Num(), MidanVehicleConstants::NumWheels);

	for (int32 i = 0; i < WheelCount; ++i)
	{
		const FWheelStatus& Status = Move->GetWheelState(i);
		if (Status.bInContact)
		{
			WorstLock = FMath::Min(WorstLock, Status.LongitudinalSlip);
		}
	}

	const float LockAmount = -WorstLock;
	if (LockAmount <= CachedConfig.ABSSlipThreshold)
	{
		return 1.f;
	}

	const float Excess = LockAmount - CachedConfig.ABSSlipThreshold;
	const float Normalised = FMath::Clamp(Excess / FMath::Max(CachedConfig.ABSSlipThreshold, KINDA_SMALL_NUMBER), 0.f, 1.f);
	return FMath::Clamp(1.f - (Normalised * CachedConfig.ABSStrength), 0.f, 1.f);
}

float UVehicleAssistComponent::ComputeStabilityMultiplier(const float ChassisSlipAngleDegrees) const
{
	const float AbsSlip = FMath::Abs(ChassisSlipAngleDegrees);
	if (AbsSlip <= CachedConfig.StabilitySlipAngleThresholdDegrees)
	{
		return 1.f;
	}

	// Ramp over a second threshold-width beyond the threshold, so intervention
	// arrives gradually. A hard cut at the threshold would produce a step change
	// in throttle mid-slide, which feels like the car breaking rather than
	// stabilising.
	const float Excess = AbsSlip - CachedConfig.StabilitySlipAngleThresholdDegrees;
	const float Normalised = FMath::Clamp(
		Excess / FMath::Max(CachedConfig.StabilitySlipAngleThresholdDegrees, KINDA_SMALL_NUMBER), 0.f, 1.f);

	return FMath::Clamp(1.f - (Normalised * CachedConfig.StabilityStrength), 0.f, 1.f);
}

float UVehicleAssistComponent::ComputeSteeringAssistDelta(
	const float CurrentSteer,
	const float ChassisSlipAngleDegrees,
	const float ForwardSpeedKmh) const
{
	// Speed floor. Not a feel dial: below this the slip angle itself is already
	// clamped to zero by GetChassisSlipAngleDegrees, so any correction here would
	// be computed from a number that does not mean anything. Kept slightly above
	// that floor so the two do not fight at the boundary.
	static constexpr float MinSpeedForAssistKmh = 15.f;

	// Slip angle treated as "fully sideways", degrees. A definition, not a
	// tuning value — it is the denominator that makes SteeringAssistStrength read
	// as a fraction of a full correction. Changing it would silently rescale
	// every authored strength value in every vehicle asset.
	static constexpr float FullySidewaysAngleDegrees = 45.f;

	if (FMath::Abs(ForwardSpeedKmh) < MinSpeedForAssistKmh)
	{
		return 0.f;
	}

	// Counter-steer proportional to slip angle, in the direction that points the
	// wheels where the car is actually going.
	const float NormalisedSlip = FMath::Clamp(ChassisSlipAngleDegrees / FullySidewaysAngleDegrees, -1.f, 1.f);
	const float Correction = NormalisedSlip * CachedConfig.SteeringAssistStrength;

	// Only ever assist toward the slide, never fight the driver's own
	// counter-steer. If the driver is already steering into the slide, adding
	// more would over-rotate and read as the car stealing control.
	if (FMath::Sign(Correction) == FMath::Sign(CurrentSteer) && FMath::Abs(CurrentSteer) > FMath::Abs(Correction))
	{
		return 0.f;
	}

	return Correction;
}

void UVehicleAssistComponent::ApplyAssists(
	FMidanVehicleInputState& InOutInput,
	const float ForwardSpeedKmh,
	const float ChassisSlipAngleDegrees)
{
	LastInterventions = EMidanAssistFlags::None;
	LastTCReduction = 0.f;
	LastABSReduction = 0.f;

	if (!bInitialised)
	{
		return;
	}

	// Order matters, and it is: traction control, ABS, stability, steering.
	//
	// TC and ABS act on different channels (throttle vs brake) so they cannot
	// conflict. Stability acts on throttle AFTER TC, so the two multiply rather
	// than compete — a car both spinning its wheels and sliding gets both
	// reductions, which is correct. Steering assist is last because it must see
	// the driver's final steer value, not an intermediate one.

	if (EnumHasAllFlags(EnabledMask, EMidanAssistFlags::TractionControl) && InOutInput.Throttle > KINDA_SMALL_NUMBER)
	{
		const float Multiplier = ComputeTractionControlMultiplier();
		if (Multiplier < 1.f - KINDA_SMALL_NUMBER)
		{
			LastTCReduction = 1.f - Multiplier;
			InOutInput.Throttle *= Multiplier;
			EnumAddFlags(LastInterventions, EMidanAssistFlags::TractionControl);
		}
	}

	if (EnumHasAllFlags(EnabledMask, EMidanAssistFlags::ABS) && InOutInput.Brake > KINDA_SMALL_NUMBER)
	{
		const float Multiplier = ComputeABSMultiplier();
		if (Multiplier < 1.f - KINDA_SMALL_NUMBER)
		{
			LastABSReduction = 1.f - Multiplier;
			InOutInput.Brake *= Multiplier;
			EnumAddFlags(LastInterventions, EMidanAssistFlags::ABS);
		}
	}

	if (EnumHasAllFlags(EnabledMask, EMidanAssistFlags::Stability) && InOutInput.Throttle > KINDA_SMALL_NUMBER)
	{
		const float Multiplier = ComputeStabilityMultiplier(ChassisSlipAngleDegrees);
		if (Multiplier < 1.f - KINDA_SMALL_NUMBER)
		{
			InOutInput.Throttle *= Multiplier;
			EnumAddFlags(LastInterventions, EMidanAssistFlags::Stability);
		}
	}

	if (EnumHasAllFlags(EnabledMask, EMidanAssistFlags::SteeringAssist))
	{
		const float Delta = ComputeSteeringAssistDelta(InOutInput.Steer, ChassisSlipAngleDegrees, ForwardSpeedKmh);
		if (!FMath::IsNearlyZero(Delta))
		{
			InOutInput.Steer = FMath::Clamp(InOutInput.Steer + Delta, -1.f, 1.f);
			EnumAddFlags(LastInterventions, EMidanAssistFlags::SteeringAssist);
		}
	}

	// Re-sanitise: three multiplications and an addition have happened since the
	// input was last validated.
	InOutInput.Sanitise();
}
