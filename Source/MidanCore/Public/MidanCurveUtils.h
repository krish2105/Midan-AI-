// Curve evaluation and validation helpers.
//
// Responsibility: safe curve access and domain validation.
// Single reason to change: the curve container type changes.
//
// Every feel and handling value in this project is a curve rather than a
// constant, which makes two things routine: evaluating a curve in a hot path
// where it may be unset, and proving at validation time that a curve actually
// covers the input range it will be asked about. A torque curve whose domain
// stops short of MaxRPM produces a car that mysteriously dies at the top end,
// and that is a validation failure, not a tuning problem.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"

struct FMidanValidationResult;

namespace MidanCurve
{
	/**
	 * Evaluate with a fallback when the curve has no keys.
	 *
	 * Hot-path safe: no allocation, no branch on an unset UObject pointer.
	 * Prefer FRuntimeFloatCurve over TObjectPtr<UCurveFloat> for anything read
	 * in a physics callback — it is inline data, so there is no indirection and
	 * no chance of an async-loaded null.
	 */
	FORCEINLINE float EvalSafe(const FRuntimeFloatCurve& Curve, float Time, float Fallback)
	{
		const FRichCurve* Rich = Curve.GetRichCurveConst();
		return (Rich && Rich->GetNumKeys() > 0) ? Rich->Eval(Time) : Fallback;
	}

	/** True when the curve has at least one key. */
	FORCEINLINE bool HasKeys(const FRuntimeFloatCurve& Curve)
	{
		const FRichCurve* Rich = Curve.GetRichCurveConst();
		return Rich && Rich->GetNumKeys() > 0;
	}

	/** Inclusive input domain. Returns false when the curve has no keys. */
	MIDANCORE_API bool GetDomain(const FRuntimeFloatCurve& Curve, float& OutMin, float& OutMax);

	/** Inclusive output range across all keys. False when the curve has no keys. */
	MIDANCORE_API bool GetValueRange(const FRuntimeFloatCurve& Curve, float& OutMin, float& OutMax);

	/**
	 * Error unless the curve covers [RequiredMin, RequiredMax].
	 *
	 * This is the check that catches a torque curve stopping short of MaxRPM or
	 * a steering curve that never reaches top speed.
	 */
	MIDANCORE_API void ValidateDomainCovers(
		const FRuntimeFloatCurve& Curve,
		float RequiredMin,
		float RequiredMax,
		FName Context,
		FMidanValidationResult& Result);

	/** Error when any sampled output falls outside [MinAllowed, MaxAllowed]. */
	MIDANCORE_API void ValidateValuesWithin(
		const FRuntimeFloatCurve& Curve,
		float MinAllowed,
		float MaxAllowed,
		FName Context,
		FMidanValidationResult& Result,
		int32 SampleCount = 32);

	/**
	 * Error when the curve rises anywhere across its domain.
	 *
	 * A steering curve must not permit more lock at higher speed. Sampled
	 * rather than key-inspected, because interpolation mode can make a curve
	 * non-monotonic between two monotonic keys.
	 */
	MIDANCORE_API void ValidateMonotonicNonIncreasing(
		const FRuntimeFloatCurve& Curve,
		FName Context,
		FMidanValidationResult& Result,
		int32 SampleCount = 32);
}
