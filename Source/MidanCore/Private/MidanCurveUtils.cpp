#include "MidanCurveUtils.h"
#include "MidanDataAsset.h"

#define LOCTEXT_NAMESPACE "MidanCurve"

namespace MidanCurve
{
	bool GetDomain(const FRuntimeFloatCurve& Curve, float& OutMin, float& OutMax)
	{
		const FRichCurve* Rich = Curve.GetRichCurveConst();
		if (!Rich || Rich->GetNumKeys() == 0)
		{
			return false;
		}
		Rich->GetTimeRange(OutMin, OutMax);
		return true;
	}

	bool GetValueRange(const FRuntimeFloatCurve& Curve, float& OutMin, float& OutMax)
	{
		const FRichCurve* Rich = Curve.GetRichCurveConst();
		if (!Rich || Rich->GetNumKeys() == 0)
		{
			return false;
		}
		Rich->GetValueRange(OutMin, OutMax);
		return true;
	}

	void ValidateDomainCovers(
		const FRuntimeFloatCurve& Curve,
		float RequiredMin,
		float RequiredMax,
		FName Context,
		FMidanValidationResult& Result)
	{
		float Min = 0.f;
		float Max = 0.f;
		if (!GetDomain(Curve, Min, Max))
		{
			Result.AddError(Context, LOCTEXT("CurveEmpty", "Curve has no keys. Every curve-driven value must be authored; an empty curve silently falls back to a constant."));
			return;
		}

		// Tolerance absorbs float authoring imprecision — a designer typing
		// 8000 for MaxRPM and placing a key at 7999.99 is not an error.
		constexpr float Tolerance = 0.01f;

		if (Min > RequiredMin + Tolerance)
		{
			Result.AddError(Context, FText::Format(
				LOCTEXT("CurveDomainMinFmt", "Curve domain starts at {0} but must cover from {1}. Inputs below the first key clamp to it, which hides the gap instead of failing."),
				FText::AsNumber(Min), FText::AsNumber(RequiredMin)));
		}

		if (Max < RequiredMax - Tolerance)
		{
			Result.AddError(Context, FText::Format(
				LOCTEXT("CurveDomainMaxFmt", "Curve domain ends at {0} but must cover up to {1}. Inputs above the last key clamp to it."),
				FText::AsNumber(Max), FText::AsNumber(RequiredMax)));
		}
	}

	void ValidateValuesWithin(
		const FRuntimeFloatCurve& Curve,
		float MinAllowed,
		float MaxAllowed,
		FName Context,
		FMidanValidationResult& Result,
		int32 SampleCount)
	{
		float DomainMin = 0.f;
		float DomainMax = 0.f;
		if (!GetDomain(Curve, DomainMin, DomainMax))
		{
			Result.AddError(Context, LOCTEXT("CurveEmptyRange", "Curve has no keys, so its output range cannot be validated."));
			return;
		}

		const int32 Samples = FMath::Max(SampleCount, 2);
		const float Span = DomainMax - DomainMin;

		for (int32 Index = 0; Index < Samples; ++Index)
		{
			const float Alpha = static_cast<float>(Index) / static_cast<float>(Samples - 1);
			const float Time = DomainMin + Span * Alpha;
			const float Value = EvalSafe(Curve, Time, 0.f);

			if (Value < MinAllowed || Value > MaxAllowed)
			{
				Result.AddError(Context, FText::Format(
					LOCTEXT("CurveValueOutOfRangeFmt", "Curve evaluates to {0} at input {1}, outside the permitted range [{2}, {3}]. Interpolation can overshoot between in-range keys, so check the tangents."),
					FText::AsNumber(Value), FText::AsNumber(Time),
					FText::AsNumber(MinAllowed), FText::AsNumber(MaxAllowed)));
				return; // One report per curve; a bad tangent would otherwise spam every sample.
			}
		}
	}

	void ValidateMonotonicNonIncreasing(
		const FRuntimeFloatCurve& Curve,
		FName Context,
		FMidanValidationResult& Result,
		int32 SampleCount)
	{
		float DomainMin = 0.f;
		float DomainMax = 0.f;
		if (!GetDomain(Curve, DomainMin, DomainMax))
		{
			Result.AddError(Context, LOCTEXT("CurveEmptyMono", "Curve has no keys, so monotonicity cannot be validated."));
			return;
		}

		const int32 Samples = FMath::Max(SampleCount, 2);
		const float Span = DomainMax - DomainMin;

		// Absolute tolerance is wrong here: a curve in degrees and a curve in
		// cm/s have different natural scales. Scale the tolerance to the
		// curve's own output range so the check behaves the same either way.
		float ValueMin = 0.f;
		float ValueMax = 0.f;
		GetValueRange(Curve, ValueMin, ValueMax);
		const float Tolerance = FMath::Max(FMath::Abs(ValueMax - ValueMin) * 1e-3f, KINDA_SMALL_NUMBER);

		float Previous = EvalSafe(Curve, DomainMin, 0.f);

		for (int32 Index = 1; Index < Samples; ++Index)
		{
			const float Alpha = static_cast<float>(Index) / static_cast<float>(Samples - 1);
			const float Time = DomainMin + Span * Alpha;
			const float Value = EvalSafe(Curve, Time, 0.f);

			if (Value > Previous + Tolerance)
			{
				Result.AddError(Context, FText::Format(
					LOCTEXT("CurveNotMonotonicFmt", "Curve rises from {0} to {1} between inputs {2} and {3}. This curve must never increase across its domain."),
					FText::AsNumber(Previous), FText::AsNumber(Value),
					FText::AsNumber(DomainMin + Span * (static_cast<float>(Index - 1) / static_cast<float>(Samples - 1))),
					FText::AsNumber(Time)));
				return;
			}

			Previous = Value;
		}
	}
}

#undef LOCTEXT_NAMESPACE
