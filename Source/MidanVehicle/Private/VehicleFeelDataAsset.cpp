#include "VehicleFeelDataAsset.h"
#include "MidanCurveUtils.h"

#define LOCTEXT_NAMESPACE "VehicleFeelDataAsset"

namespace
{
	/**
	 * Every speed-driven curve is normalised, so its domain must cover 0..1.
	 * A curve authored against raw km/h instead would silently clamp to its last
	 * key and appear to work at low speed only — a nasty class of feel bug,
	 * because nothing errors and the effect just stops scaling.
	 */
	void ValidateNormalisedCurve(
		const FRuntimeFloatCurve& Curve,
		const TCHAR* FieldName,
		FMidanValidationResult& Result,
		bool bRequired)
	{
		const FName Context(FieldName);

		if (!MidanCurve::HasKeys(Curve))
		{
			if (bRequired)
			{
				Result.AddError(Context, FText::Format(
					LOCTEXT("FeelCurveMissingFmt", "{0} has no keys. Feel values must be curves, not constants — a constant is right at one speed and wrong everywhere else."),
					FText::FromString(FieldName)));
			}
			else
			{
				Result.AddWarning(Context, FText::Format(
					LOCTEXT("FeelCurveEmptyFmt", "{0} has no keys, so this channel contributes nothing."),
					FText::FromString(FieldName)));
			}
			return;
		}

		float Min = 0.f;
		float Max = 0.f;
		MidanCurve::GetDomain(Curve, Min, Max);

		if (Max > 1.5f)
		{
			Result.AddError(Context, FText::Format(
				LOCTEXT("FeelCurveNotNormalisedFmt", "{0} has a domain reaching {1}, which suggests it was authored against raw km/h. This curve's input is NORMALISED speed 0..1 — divide by SpeedNormalisationKmh. As authored it clamps above its last key and stops scaling."),
				FText::FromString(FieldName), FText::AsNumber(Max)));
		}

		MidanCurve::ValidateDomainCovers(Curve, 0.f, 1.f, Context, Result);
	}

	void ValidateCameraMode(
		const FMidanCameraModeConfig& Mode,
		const TCHAR* ModeName,
		FMidanValidationResult& Result)
	{
		const FName Context(ModeName);

		if (Mode.BaseFOVDegrees <= 0.f)
		{
			Result.AddError(Context, LOCTEXT("FovNonPositive", "Base FOV must be positive."));
		}

		// The FOV response curve is the single biggest speed-sensation lever, so
		// its absence is an error rather than a warning.
		if (!MidanCurve::HasKeys(Mode.FOVOffsetBySpeed))
		{
			Result.AddError(Context, FText::Format(
				LOCTEXT("NoFovCurveFmt", "{0} has no FOV response curve. This is the single biggest speed-sensation lever available — without it the camera FOV is static and the car will not feel fast however quickly it moves."),
				FText::FromString(ModeName)));
		}
		else
		{
			float Min = 0.f;
			float Max = 0.f;
			MidanCurve::GetDomain(Mode.FOVOffsetBySpeed, Min, Max);
			if (Max > 1.5f)
			{
				Result.AddError(Context, FText::Format(
					LOCTEXT("FovCurveNotNormalisedFmt", "{0} FOV curve domain reaches {1}; its input is normalised speed 0..1, not km/h."),
					FText::FromString(ModeName), FText::AsNumber(Max)));
			}

			float ValueMin = 0.f;
			float ValueMax = 0.f;
			MidanCurve::GetValueRange(Mode.FOVOffsetBySpeed, ValueMin, ValueMax);

			const float PeakFOV = Mode.BaseFOVDegrees + ValueMax;
			if (PeakFOV > 130.f)
			{
				Result.AddWarning(Context, FText::Format(
					LOCTEXT("FovTooWideFmt", "{0} reaches {1}° at top speed. Above about 120° the distortion reads as a fisheye lens rather than speed."),
					FText::FromString(ModeName), FText::AsNumber(PeakFOV)));
			}
			if (ValueMax < 5.f)
			{
				Result.AddWarning(Context, FText::Format(
					LOCTEXT("FovTooNarrowFmt", "{0} only widens by {1}° at top speed. ART_DIRECTION §5 calls for 72° reaching 95-100°, so roughly 23-28° of offset."),
					FText::FromString(ModeName), FText::AsNumber(ValueMax)));
			}
		}

		if (Mode.ArmLengthCm < 0.f)
		{
			Result.AddError(Context, LOCTEXT("ArmNegative", "Spring arm length cannot be negative."));
		}
	}
}

FPrimaryAssetId UVehicleFeelDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("MidanVehicleFeel"), GetFName());
}

void UVehicleFeelDataAsset::ValidateMidanData(FMidanValidationResult& Result) const
{
	if (SpeedNormalisationKmh <= 0.f)
	{
		Result.AddError(TEXT("SpeedNormalisationKmh"),
			LOCTEXT("NormalisationNonPositive", "Speed normalisation must be positive; every speed-driven curve divides by it."));
	}

	ValidateCameraMode(ChaseFar, TEXT("ChaseFar"), Result);
	ValidateCameraMode(ChaseNear, TEXT("ChaseNear"), Result);
	ValidateCameraMode(Bonnet, TEXT("Bonnet"), Result);
	ValidateCameraMode(Cockpit, TEXT("Cockpit"), Result);

	// Lag curves are required: without lag the camera is rigidly attached and
	// the car reads as weightless.
	ValidateNormalisedCurve(ChaseFar.LocationLagBySpeed, TEXT("ChaseFar.LocationLagBySpeed"), Result, true);
	ValidateNormalisedCurve(ChaseFar.RotationLagBySpeed, TEXT("ChaseFar.RotationLagBySpeed"), Result, true);

	ValidateNormalisedCurve(WindGainBySpeed, TEXT("WindGainBySpeed"), Result, false);
	ValidateNormalisedCurve(ChromaticAberrationBySpeed, TEXT("ChromaticAberrationBySpeed"), Result, false);
	ValidateNormalisedCurve(VignetteBySpeed, TEXT("VignetteBySpeed"), Result, false);
	ValidateNormalisedCurve(MotionBlurBySpeed, TEXT("MotionBlurBySpeed"), Result, false);

	// Slip-driven curves take 0..1 slip, so the same normalisation rule applies.
	ValidateNormalisedCurve(TyreScrubGainBySlip, TEXT("TyreScrubGainBySlip"), Result, false);
	ValidateNormalisedCurve(TyreSmokeRateBySlip, TEXT("TyreSmokeRateBySlip"), Result, false);

	// Post-process intensities are 0..1 in the engine; an out-of-range value is
	// silently clamped, which looks like the curve not working.
	if (MidanCurve::HasKeys(ChromaticAberrationBySpeed))
	{
		MidanCurve::ValidateValuesWithin(ChromaticAberrationBySpeed, 0.f, 1.f,
			TEXT("ChromaticAberrationBySpeed"), Result);
	}
	if (MidanCurve::HasKeys(VignetteBySpeed))
	{
		MidanCurve::ValidateValuesWithin(VignetteBySpeed, 0.f, 1.f, TEXT("VignetteBySpeed"), Result);
	}
	if (MidanCurve::HasKeys(MotionBlurBySpeed))
	{
		MidanCurve::ValidateValuesWithin(MotionBlurBySpeed, 0.f, 1.f, TEXT("MotionBlurBySpeed"), Result);
	}

	// Decal pooling. An unbounded pool is a slow leak that appears on lap four,
	// so a zero pool size with a material assigned is worth flagging.
	if (SkidDecalPoolSize <= 0 && !SkidDecalMaterial.IsNull())
	{
		Result.AddWarning(TEXT("SkidDecalPoolSize"),
			LOCTEXT("DecalPoolZero", "A skid decal material is assigned but the pool size is zero, so no decals will ever appear."));
	}

	if (EngineSound.IsNull())
	{
		Result.AddWarning(TEXT("EngineSound"),
			LOCTEXT("NoEngineSound", "No engine MetaSound assigned. Engine audio is the largest single audio contributor to how fast a car feels."));
	}
}

#undef LOCTEXT_NAMESPACE
