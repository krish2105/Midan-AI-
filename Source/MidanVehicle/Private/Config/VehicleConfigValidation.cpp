// Validation for every vehicle config struct.
//
// All eight live in one translation unit deliberately: the rules are short,
// they share the same LOCTEXT namespace and helpers, and keeping them together
// makes it possible to read the entire physical-sanity contract in one pass.
// Splitting them across eight .cpp files would hide exactly that.
//
// Error vs warning is a real distinction here. An error means the value is
// physically impossible or internally contradictory. A warning means it is
// unusual and might be intentional — a high centre of mass on the rally car is
// the canonical case, and erroring on it would make the validator fight the
// design it exists to protect.

#include "Config/VehicleMassConfig.h"
#include "Config/VehiclePowertrainConfig.h"
#include "Config/VehicleDrivetrainConfig.h"
#include "Config/VehicleSuspensionConfig.h"
#include "Config/VehicleTyreConfig.h"
#include "Config/VehicleSteeringConfig.h"
#include "Config/VehicleAeroConfig.h"
#include "Config/VehicleAssistConfig.h"

#include "MidanDataAsset.h"
#include "MidanCurveUtils.h"
#include "MidanMathUtils.h"

#define LOCTEXT_NAMESPACE "MidanVehicleConfig"

namespace
{
	/** Compose a nested context name so a finding points at the exact field. */
	FName Sub(FName Context, const TCHAR* Leaf)
	{
		return FName(*FString::Printf(TEXT("%s.%s"), *Context.ToString(), Leaf));
	}
}

// ---------------------------------------------------------------------------
// Mass
// ---------------------------------------------------------------------------

void FVehicleMassConfig::Validate(FName Context, FMidanValidationResult& Result) const
{
	if (MassKg <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("MassKg")),
			LOCTEXT("MassNonPositive", "Mass must be positive. Zero or negative mass makes the physics solver produce NaN, which surfaces as the car vanishing."));
	}
	else if (MassKg < 300.f || MassKg > 3500.f)
	{
		Result.AddWarning(Sub(Context, TEXT("MassKg")), FText::Format(
			LOCTEXT("MassImplausibleFmt", "Mass of {0}kg is outside the plausible range for a road car (300-3500kg). Spring rates and downforce are tuned against mass, so an implausible value invalidates both."),
			FText::AsNumber(MassKg)));
	}

	if (InertiaTensorScale.X <= 0.f || InertiaTensorScale.Y <= 0.f || InertiaTensorScale.Z <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("InertiaTensorScale")),
			LOCTEXT("InertiaNonPositive", "Every inertia tensor scale component must be positive. A zero component means the chassis has no resistance to rotation about that axis."));
	}

	// Warning, never an error. VEHICLE_SPEC calls for a deliberately high COM on
	// the rally car; the validator must not argue with that.
	if (CentreOfMassOffset.Z > 30.f)
	{
		Result.AddWarning(Sub(Context, TEXT("CentreOfMassOffset")), FText::Format(
			LOCTEXT("ComHighFmt", "Centre of mass is {0}cm above the chassis origin. This is a design dial, so it may be intentional (the rally car wants it) — but if the car flips unexpectedly, check physics-asset bone weighting before lowering this."),
			FText::AsNumber(CentreOfMassOffset.Z)));
	}

	if (FMath::Abs(CentreOfMassOffset.Y) > 15.f)
	{
		Result.AddWarning(Sub(Context, TEXT("CentreOfMassOffset")),
			LOCTEXT("ComLateral", "Centre of mass is significantly off-centre laterally. The car will pull to one side and corner asymmetrically. Intentional only for a deliberately broken setup."));
	}
}

// ---------------------------------------------------------------------------
// Powertrain
// ---------------------------------------------------------------------------

void FVehiclePowertrainConfig::Validate(FName Context, FMidanValidationResult& Result) const
{
	if (EngineIdleRPM >= MaxRPM)
	{
		Result.AddError(Sub(Context, TEXT("EngineIdleRPM")), FText::Format(
			LOCTEXT("IdleAboveMaxFmt", "Idle RPM ({0}) must be below max RPM ({1}). There is no usable rev band otherwise."),
			FText::AsNumber(EngineIdleRPM), FText::AsNumber(MaxRPM)));
	}
	else
	{
		// The check that catches a car mysteriously dying at the top end.
		MidanCurve::ValidateDomainCovers(TorqueCurve, EngineIdleRPM, MaxRPM,
			Sub(Context, TEXT("TorqueCurve")), Result);
	}

	if (MidanCurve::HasKeys(TorqueCurve))
	{
		float TorqueMin = 0.f;
		float TorqueMax = 0.f;
		MidanCurve::GetValueRange(TorqueCurve, TorqueMin, TorqueMax);

		if (TorqueMin < 0.f)
		{
			Result.AddError(Sub(Context, TEXT("TorqueCurve")), FText::Format(
				LOCTEXT("TorqueNegativeFmt", "Torque curve reaches {0}Nm. Negative drive torque is not engine braking — that is EngineBrakeEffect. Check the curve tangents, which can undershoot below in-range keys."),
				FText::AsNumber(TorqueMin)));
		}

		if (TorqueMax <= 0.f)
		{
			Result.AddError(Sub(Context, TEXT("TorqueCurve")),
				LOCTEXT("TorqueAllZero", "Torque curve never exceeds zero. The car cannot move."));
		}
	}

	if (ForwardGearRatios.Num() < 2)
	{
		Result.AddError(Sub(Context, TEXT("ForwardGearRatios")),
			LOCTEXT("TooFewGears", "At least two forward gears are required. Gear spacing controls how often the player feels an event, and a single gear removes that entirely."));
	}

	for (int32 Index = 0; Index < ForwardGearRatios.Num(); ++Index)
	{
		if (ForwardGearRatios[Index] <= 0.f)
		{
			Result.AddError(Sub(Context, TEXT("ForwardGearRatios")), FText::Format(
				LOCTEXT("GearNonPositiveFmt", "Gear {0} has ratio {1}. Every forward ratio must be positive."),
				FText::AsNumber(Index + 1), FText::AsNumber(ForwardGearRatios[Index])));
		}

		if (Index > 0 && ForwardGearRatios[Index] >= ForwardGearRatios[Index - 1])
		{
			Result.AddError(Sub(Context, TEXT("ForwardGearRatios")), FText::Format(
				LOCTEXT("GearsNotMonotonicFmt", "Gear {0} ({1}) is not lower than gear {2} ({3}). Ratios must decrease monotonically, highest first — an out-of-order ratio makes the automatic gearbox oscillate between two gears."),
				FText::AsNumber(Index + 1), FText::AsNumber(ForwardGearRatios[Index]),
				FText::AsNumber(Index), FText::AsNumber(ForwardGearRatios[Index - 1])));
		}
	}

	if (FinalDriveRatio <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("FinalDriveRatio")),
			LOCTEXT("FinalDriveNonPositive", "Final drive ratio must be positive."));
	}

	if (ReverseGearRatio <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("ReverseGearRatio")),
			LOCTEXT("ReverseNonPositive", "Reverse gear ratio must be positive. Direction is handled by the gearbox, not by the sign of the ratio."));
	}

	if (AutoShiftDownRatio >= AutoShiftUpRatio)
	{
		Result.AddError(Sub(Context, TEXT("AutoShiftDownRatio")), FText::Format(
			LOCTEXT("ShiftHysteresisFmt", "Auto shift-down ratio ({0}) must be below shift-up ratio ({1}). Without hysteresis the gearbox hunts between two gears continuously."),
			FText::AsNumber(AutoShiftDownRatio), FText::AsNumber(AutoShiftUpRatio)));
	}

	if (ChangeUpTime > 0.4f || ChangeDownTime > 0.4f)
	{
		Result.AddWarning(Sub(Context, TEXT("ChangeUpTime")),
			LOCTEXT("ShiftSlow", "Shift time above 0.4s reads as an old automatic rather than a modern dual-clutch. Valid if intentional; sub-0.15s is the modern feel."));
	}
}

// ---------------------------------------------------------------------------
// Drivetrain
// ---------------------------------------------------------------------------

void FVehicleDrivetrainConfig::Validate(FName Context, FMidanValidationResult& Result) const
{
	// Torque split only means anything on AWD; on FWD/RWD it is ignored by the
	// applier, so a surprising value there is worth flagging as a likely
	// misunderstanding rather than silently discarding.
	if (Layout != EMidanDrivetrainLayout::AllWheelDrive
		&& !FMath::IsNearlyEqual(FrontTorqueSplit, 0.4f))
	{
		Result.AddWarning(Sub(Context, TEXT("FrontTorqueSplit")),
			LOCTEXT("SplitIgnored", "Front torque split is set but the layout is not AWD, so it has no effect. Change the layout or reset the split to avoid implying a distribution that does not exist."));
	}

	if (Layout == EMidanDrivetrainLayout::AllWheelDrive)
	{
		if (FMath::IsNearlyZero(FrontTorqueSplit) || FMath::IsNearlyEqual(FrontTorqueSplit, 1.f))
		{
			Result.AddWarning(Sub(Context, TEXT("FrontTorqueSplit")),
				LOCTEXT("SplitDegenerate", "An AWD split of 0.0 or 1.0 is effectively RWD or FWD. Set the layout explicitly instead, so the intent is readable."));
		}
	}
}

// ---------------------------------------------------------------------------
// Suspension
// ---------------------------------------------------------------------------

void FVehicleAxleSuspensionConfig::Validate(FName Context, FMidanValidationResult& Result) const
{
	if (MaxRaiseCm + MaxDropCm <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("Travel")),
			LOCTEXT("NoTravel", "Total suspension travel is zero. The wheel is rigidly attached, so every bump becomes an impact and the car skates rather than absorbing."));
	}

	if (SpringRate <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("SpringRate")),
			LOCTEXT("SpringNonPositive", "Spring rate must be positive."));
	}

	if (DampingRatio <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("DampingRatio")),
			LOCTEXT("DampingNonPositive", "Damping ratio must be positive. Zero damping oscillates forever."));
	}
	else if (DampingRatio > 1.3f)
	{
		Result.AddWarning(Sub(Context, TEXT("DampingRatio")), FText::Format(
			LOCTEXT("OverDampedFmt", "Damping ratio {0} is heavily over-damped. This produces a car that is fast and feels DEAD — the most common way to ruin handling, and usually a symptom of damping out instability instead of finding its cause."),
			FText::AsNumber(DampingRatio)));
	}
	else if (DampingRatio < 0.25f)
	{
		Result.AddWarning(Sub(Context, TEXT("DampingRatio")), FText::Format(
			LOCTEXT("UnderDampedFmt", "Damping ratio {0} is heavily under-damped. The car will oscillate after every input and feel like a boat."),
			FText::AsNumber(DampingRatio)));
	}
}

void FVehicleSuspensionConfig::Validate(FName Context, FMidanValidationResult& Result) const
{
	Front.Validate(Sub(Context, TEXT("Front")), Result);
	Rear.Validate(Sub(Context, TEXT("Rear")), Result);

	const float TotalLoad = Front.WheelLoadRatio + Rear.WheelLoadRatio;
	if (!FMath::IsNearlyEqual(TotalLoad, 1.f, 0.02f))
	{
		Result.AddError(Sub(Context, TEXT("WheelLoadRatio")), FText::Format(
			LOCTEXT("LoadRatioSumFmt", "Front and rear wheel load ratios sum to {0}, not 1.0. The chassis mass must be fully distributed or the axles carry the wrong load and every tyre value is calibrated against a false baseline."),
			FText::AsNumber(TotalLoad)));
	}
}

// ---------------------------------------------------------------------------
// Tyres
// ---------------------------------------------------------------------------

void FVehicleAxleTyreConfig::Validate(FName Context, FMidanValidationResult& Result) const
{
	if (FrictionForceMultiplier <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("FrictionForceMultiplier")),
			LOCTEXT("FrictionNonPositive", "Friction multiplier must be positive. Zero means the axle has no grip at all."));
	}

	if (WheelRadiusCm <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("WheelRadiusCm")),
			LOCTEXT("RadiusNonPositive", "Wheel radius must be positive."));
	}

	if (WheelMassKg <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("WheelMassKg")),
			LOCTEXT("WheelMassNonPositive", "Wheel mass must be positive."));
	}

	if (MaxBrakeTorque < 0.f)
	{
		Result.AddError(Sub(Context, TEXT("MaxBrakeTorque")),
			LOCTEXT("BrakeNegative", "Brake torque cannot be negative."));
	}

	if (MidanCurve::HasKeys(LateralSlipGraph))
	{
		// Sampled across the authored domain: a negative lateral coefficient
		// would push the car outward in a corner.
		MidanCurve::ValidateValuesWithin(LateralSlipGraph, 0.f, 10.f,
			Sub(Context, TEXT("LateralSlipGraph")), Result);
	}
	else
	{
		Result.AddWarning(Sub(Context, TEXT("LateralSlipGraph")),
			LOCTEXT("SlipGraphEmpty", "Lateral slip graph has no keys, so Chaos will use its default. This curve's SHAPE is what makes slides progressive and catchable rather than snappy — authoring it is most of the tyre feel."));
	}

	if (SkidThreshold < SlipThreshold)
	{
		Result.AddWarning(Sub(Context, TEXT("SkidThreshold")),
			LOCTEXT("SkidBelowSlip", "Skid feedback threshold is below the traction-control slip threshold, so smoke and audio begin before TC intervenes. Usually the reverse is intended."));
	}
}

void FVehicleTyreConfig::Validate(FName Context, FMidanValidationResult& Result) const
{
	Front.Validate(Sub(Context, TEXT("Front")), Result);
	Rear.Validate(Sub(Context, TEXT("Rear")), Result);

	// The balance number itself. Not an error at any value — extreme balance is
	// a legitimate design choice — but a car this far from neutral is almost
	// certainly a typo rather than a decision.
	const float Ratio = GetFrontRearFrictionRatio();
	if (Ratio > KINDA_SMALL_NUMBER && (Ratio < 0.6f || Ratio > 1.6f))
	{
		Result.AddWarning(Sub(Context, TEXT("FrontRearFrictionRatio")), FText::Format(
			LOCTEXT("BalanceExtremeFmt", "Front/rear friction ratio is {0}. Above 1.0 is oversteer, below is understeer, but beyond 0.6-1.6 the car is likely undriveable rather than characterful. This is the single number to sweep when tuning balance."),
			FText::AsNumber(Ratio)));
	}

	const float FrontBias = GetFrontBrakeBias();
	if (FrontBias > KINDA_SMALL_NUMBER && FrontBias < 0.5f)
	{
		Result.AddWarning(Sub(Context, TEXT("MaxBrakeTorque")), FText::Format(
			LOCTEXT("BrakeBiasRearFmt", "Front brake bias is {0}, so the rear axle brakes harder than the front. This makes the car unstable and prone to spinning under heavy braking."),
			FText::AsNumber(FrontBias)));
	}

	if (Front.MaxHandbrakeTorque > KINDA_SMALL_NUMBER)
	{
		Result.AddWarning(Sub(Context, TEXT("Front.MaxHandbrakeTorque")),
			LOCTEXT("HandbrakeFront", "Front handbrake torque is non-zero. A handbrake acts on the rear axle; front handbrake makes the car understeer under handbrake, which is the opposite of the intended effect."));
	}

	if (Rear.MaxHandbrakeTorque <= KINDA_SMALL_NUMBER)
	{
		Result.AddWarning(Sub(Context, TEXT("Rear.MaxHandbrakeTorque")),
			LOCTEXT("HandbrakeMissing", "Rear handbrake torque is zero, so the handbrake input does nothing."));
	}
}

// ---------------------------------------------------------------------------
// Steering
// ---------------------------------------------------------------------------

void FVehicleSteeringConfig::Validate(FName Context, FMidanValidationResult& Result) const
{
	if (MaxSteerAngleDegrees <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("MaxSteerAngleDegrees")),
			LOCTEXT("SteerAngleNonPositive", "Max steer angle must be positive."));
	}

	// The mandatory curve. Its absence is an error, not a warning — a linear
	// input-to-angle map is the single most common reason a UE5 car feels wrong.
	if (!MidanCurve::HasKeys(SteeringCurve))
	{
		Result.AddError(Sub(Context, TEXT("SteeringCurve")),
			LOCTEXT("SteeringCurveMissing", "The steering curve is mandatory and has no keys. Without it, raw input maps directly to steer angle — full lock at 250km/h is an instant spin, and the angle that suits a hairpin is unusable on a straight."));
	}
	else
	{
		MidanCurve::ValidateDomainCovers(SteeringCurve, 0.f, ValidationTopSpeedKmh,
			Sub(Context, TEXT("SteeringCurve")), Result);

		MidanCurve::ValidateValuesWithin(SteeringCurve, 0.f, 1.f,
			Sub(Context, TEXT("SteeringCurve")), Result);

		// Permitting more lock at higher speed is never correct.
		MidanCurve::ValidateMonotonicNonIncreasing(SteeringCurve,
			Sub(Context, TEXT("SteeringCurve")), Result);
	}

	if (!MidanCurve::HasKeys(KeyboardShapingCurve))
	{
		Result.AddWarning(Sub(Context, TEXT("KeyboardShapingCurve")),
			LOCTEXT("KeyboardCurveMissing", "No keyboard shaping curve. Digital keys through the analogue path is a distinct failure — the input jumps to full deflection instantly. Ramp in over roughly 100-200ms with its own shape."));
	}
	else
	{
		MidanCurve::ValidateValuesWithin(KeyboardShapingCurve, 0.f, 1.f,
			Sub(Context, TEXT("KeyboardShapingCurve")), Result);
	}

	if (SteeringInputRiseRate <= 0.f || SteeringInputFallRate <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("SteeringInputRates")),
			LOCTEXT("SteerRatesNonPositive", "Steering rise and fall rates must both be positive, or the steering never reaches its target."));
	}
	else if (FMath::IsNearlyEqual(SteeringInputRiseRate, SteeringInputFallRate))
	{
		Result.AddWarning(Sub(Context, TEXT("SteeringInputRates")),
			LOCTEXT("SteerRatesSymmetric", "Rise and fall rates are equal. Asymmetric rates are the point: fast attack with slower release reads as responsive but stable, and one symmetric rate satisfies neither."));
	}
}

// ---------------------------------------------------------------------------
// Aero
// ---------------------------------------------------------------------------

void FVehicleAeroConfig::Validate(FName Context, FMidanValidationResult& Result) const
{
	if (!bEnabled)
	{
		return;
	}

	if (FrontalAreaM2 <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("FrontalAreaM2")),
			LOCTEXT("AreaNonPositive", "Frontal area must be positive; it scales both drag and downforce."));
	}

	if (DragCoefficient < 0.f || FrontLiftCoefficient < 0.f || RearLiftCoefficient < 0.f)
	{
		Result.AddError(Sub(Context, TEXT("Coefficients")),
			LOCTEXT("AeroNegative", "Aero coefficients cannot be negative. Negative lift coefficient would generate upforce and lift the car off the road."));
	}

	if (MaxDownforceAsWeightMultiple <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("MaxDownforceAsWeightMultiple")),
			LOCTEXT("ClampNonPositive", "The downforce safety clamp must be positive, or all downforce is suppressed."));
	}

	// The application geometry is the whole reason for implementing aero by
	// hand — if front and rear points are not on opposite sides of the origin,
	// the speed-dependent balance shift does not happen and the effort is wasted.
	if (FrontApplicationOffset.X <= 0.f)
	{
		Result.AddWarning(Sub(Context, TEXT("FrontApplicationOffset")),
			LOCTEXT("FrontAeroBehind", "Front downforce application point is not ahead of the chassis origin, so it contributes nothing to front-end bite at speed."));
	}

	if (RearApplicationOffset.X >= 0.f)
	{
		Result.AddWarning(Sub(Context, TEXT("RearApplicationOffset")),
			LOCTEXT("RearAeroAhead", "Rear downforce application point is not behind the chassis origin, so front and rear downforce are not separated and the balance cannot shift with speed."));
	}

	if (FrontLiftCoefficient <= KINDA_SMALL_NUMBER && RearLiftCoefficient <= KINDA_SMALL_NUMBER)
	{
		Result.AddWarning(Sub(Context, TEXT("Downforce")),
			LOCTEXT("NoDownforce", "Both lift coefficients are zero, so the car generates no downforce. Valid for the rally car; for a hypercar it removes the planted-at-speed character."));
	}
}

// ---------------------------------------------------------------------------
// Assists
// ---------------------------------------------------------------------------

void FVehicleAssistConfig::Validate(FName Context, FMidanValidationResult& Result) const
{
	if (bTractionControlEnabled && TractionControlSlipThreshold <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("TractionControlSlipThreshold")),
			LOCTEXT("TCThresholdNonPositive", "Traction control threshold must be positive, or torque is cut permanently and the car cannot accelerate."));
	}

	if (bABSEnabled && ABSSlipThreshold <= 0.f)
	{
		Result.AddError(Sub(Context, TEXT("ABSSlipThreshold")),
			LOCTEXT("ABSThresholdNonPositive", "ABS threshold must be positive, or brake torque is suppressed permanently and the car cannot stop."));
	}

	if (bStabilityControlEnabled && StabilitySlipAngleThresholdDegrees < 8.f)
	{
		Result.AddWarning(Sub(Context, TEXT("StabilitySlipAngleThresholdDegrees")),
			LOCTEXT("StabilityIntrusive", "A stability threshold below 8 degrees intervenes during ordinary cornering and will cancel the oversteer character the GT car exists to have."));
	}

	if (bSteeringAssistEnabled && SteeringAssistStrength > 0.3f)
	{
		Result.AddWarning(Sub(Context, TEXT("SteeringAssistStrength")),
			LOCTEXT("SteerAssistStrong", "Steering assist above 0.3 is perceptible as the car steering itself, which reads to the player as the controls being taken away."));
	}
}

#undef LOCTEXT_NAMESPACE
