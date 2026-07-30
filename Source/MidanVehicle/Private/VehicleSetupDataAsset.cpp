#include "VehicleSetupDataAsset.h"
#include "MidanCurveUtils.h"
#include "MidanGameplayTags.h"

#define LOCTEXT_NAMESPACE "VehicleSetupDataAsset"

float UVehicleSetupDataAsset::GetPeakTorquePerTonne() const
{
	if (Mass.MassKg <= 0.f || !MidanCurve::HasKeys(Powertrain.TorqueCurve))
	{
		return 0.f;
	}

	float TorqueMin = 0.f;
	float TorqueMax = 0.f;
	MidanCurve::GetValueRange(Powertrain.TorqueCurve, TorqueMin, TorqueMax);

	return TorqueMax / (Mass.MassKg / 1000.f);
}

FPrimaryAssetId UVehicleSetupDataAsset::GetPrimaryAssetId() const
{
	// Fixed type so the asset manager can scan and validate every vehicle in one
	// pass, which is what lets CI fail on a bad setup without loading a level.
	return FPrimaryAssetId(TEXT("MidanVehicle"), GetFName());
}

void UVehicleSetupDataAsset::ValidateMidanData(FMidanValidationResult& Result) const
{
	// Identity ---------------------------------------------------------------

	if (DisplayName.IsEmpty())
	{
		Result.AddError(TEXT("DisplayName"),
			LOCTEXT("NoDisplayName", "Display name is empty. Every vehicle needs a fictional name for the select screen and the results table."));
	}

	if (!VehicleClass.IsValid())
	{
		Result.AddError(TEXT("VehicleClass"),
			LOCTEXT("NoVehicleClass", "Vehicle class tag is unset. AI difficulty and the class-based rubber-band envelope both key off it."));
	}
	else if (!VehicleClass.MatchesTag(MidanTags::Vehicle_Class))
	{
		// Tag comparison via MatchesTag, not a string prefix check — which is
		// the reason MidanGameplayTags had to move up to this phase.
		Result.AddError(TEXT("VehicleClass"), FText::Format(
			LOCTEXT("BadVehicleClassFmt", "Vehicle class '{0}' does not descend from Vehicle.Class."),
			FText::FromName(VehicleClass.GetTagName())));
	}
	else if (VehicleClass == MidanTags::Vehicle_Class)
	{
		Result.AddError(TEXT("VehicleClass"),
			LOCTEXT("ParentTagAssigned", "Vehicle.Class is a parent tag and cannot be assigned directly. Use Vehicle.Class.Hyper, .GT or .Rally."));
	}

	// Nested config ----------------------------------------------------------
	// Each struct appends into the same result, so a designer sees every problem
	// at once rather than fixing them one compile at a time.

	Mass.Validate(TEXT("Mass"), Result);
	Powertrain.Validate(TEXT("Powertrain"), Result);
	Drivetrain.Validate(TEXT("Drivetrain"), Result);
	Suspension.Validate(TEXT("Suspension"), Result);
	Tyres.Validate(TEXT("Tyres"), Result);
	Steering.Validate(TEXT("Steering"), Result);
	Aero.Validate(TEXT("Aero"), Result);
	Assists.Validate(TEXT("Assists"), Result);

	// Cross-struct rules -----------------------------------------------------
	// These are the checks no individual struct can make, because they compare
	// values that live in different ones. They are the reason validation is a
	// method on the asset rather than only on the structs.

	if (Mass.MassKg > 0.f)
	{
		// Spring rate is meaningless in isolation; it is only right or wrong
		// relative to the mass it carries.
		const float TotalSpringRate = Suspension.Front.SpringRate + Suspension.Rear.SpringRate;
		const float RatePerKg = TotalSpringRate / Mass.MassKg;

		if (RatePerKg < 0.05f)
		{
			Result.AddWarning(TEXT("Suspension.SpringRate"), FText::Format(
				LOCTEXT("SpringSoftForMassFmt", "Combined spring rate {0} is very soft for {1}kg. The car will bottom out and wallow. Spring rates must be scaled against mass — a rate that suits 1250kg wallows at 1550kg."),
				FText::AsNumber(TotalSpringRate), FText::AsNumber(Mass.MassKg)));
		}
		else if (RatePerKg > 1.0f)
		{
			Result.AddWarning(TEXT("Suspension.SpringRate"), FText::Format(
				LOCTEXT("SpringStiffForMassFmt", "Combined spring rate {0} is very stiff for {1}kg. Body roll will be suppressed, and body roll is how the player reads grip."),
				FText::AsNumber(TotalSpringRate), FText::AsNumber(Mass.MassKg)));
		}
	}

	// A drivetrain layout that contradicts the class tag is almost always a
	// copy-paste error between vehicle assets.
	if (VehicleClass == MidanTags::Vehicle_Class_GT
		&& Drivetrain.Layout != EMidanDrivetrainLayout::RearWheelDrive)
	{
		Result.AddWarning(TEXT("Drivetrain.Layout"),
			LOCTEXT("GTNotRWD", "The GT class is specified as RWD in docs/VEHICLE_SPEC.md — it is the drift car, and RWD plus high torque is where the oversteer character comes from."));
	}

	if (VehicleClass == MidanTags::Vehicle_Class_Rally
		&& Drivetrain.Layout != EMidanDrivetrainLayout::AllWheelDrive)
	{
		Result.AddWarning(TEXT("Drivetrain.Layout"),
			LOCTEXT("RallyNotAWD", "The Rally class is specified as AWD in docs/VEHICLE_SPEC.md — AWD traction is what makes it unstoppable on gravel."));
	}

	if (VehicleClass == MidanTags::Vehicle_Class_Hyper
		&& Aero.bEnabled
		&& Aero.RearLiftCoefficient <= KINDA_SMALL_NUMBER)
	{
		Result.AddWarning(TEXT("Aero.RearLiftCoefficient"),
			LOCTEXT("HyperNoRearAero", "The hypercar is specified as high-downforce and planted above 150km/h. With no rear downforce that speed-dependent character does not exist."));
	}

	// The steering curve must cover the speed the car can actually reach, and
	// only the powertrain and aero know what that is.
	if (Steering.ValidationTopSpeedKmh > 0.f && Powertrain.ForwardGearRatios.Num() > 0)
	{
		const float TopGear = Powertrain.ForwardGearRatios.Last();
		if (TopGear > 0.f && Powertrain.FinalDriveRatio > 0.f && Tyres.Rear.WheelRadiusCm > 0.f)
		{
			// v = (RPM / 60) / (gear * final) * circumference
			const float WheelCircumferenceCm = 2.f * PI * Tyres.Rear.WheelRadiusCm;
			const float WheelRevsPerSec = (Powertrain.MaxRPM / 60.f) / (TopGear * Powertrain.FinalDriveRatio);
			const float TheoreticalTopSpeedKmh = WheelRevsPerSec * WheelCircumferenceCm * 0.036f;

			if (TheoreticalTopSpeedKmh > Steering.ValidationTopSpeedKmh * 1.05f)
			{
				Result.AddError(TEXT("Steering.ValidationTopSpeedKmh"), FText::Format(
					LOCTEXT("TopSpeedUnderestimatedFmt", "Gearing permits roughly {0}km/h but the steering curve is only validated to {1}km/h. Above the curve's last key the steer limit clamps, so the car silently gains lock at the highest speeds — exactly where it is most dangerous."),
					FText::AsNumber(FMath::RoundToInt(TheoreticalTopSpeedKmh)),
					FText::AsNumber(FMath::RoundToInt(Steering.ValidationTopSpeedKmh))));
			}
		}
	}

	if (Feel.IsNull())
	{
		Result.AddWarning(TEXT("Feel"),
			LOCTEXT("NoFeelAsset", "No feel asset assigned. The vehicle will drive but have no camera response, engine audio, tyre FX or haptics — roughly 40% of how it is perceived."));
	}

	if (ChassisMesh.IsNull())
	{
		Result.AddWarning(TEXT("ChassisMesh"),
			LOCTEXT("NoChassisMesh", "No chassis mesh assigned. Required before this vehicle can spawn; see docs/MANUAL_STEPS.md for the Blender rig and import steps."));
	}
}

#undef LOCTEXT_NAMESPACE
