// Small math helpers shared across modules. Header-only, allocation-free.
//
// Responsibility: numerical utilities used in hot paths.
// Single reason to change: never, in practice — these are definitions, not policy.

#pragma once

#include "CoreMinimal.h"

namespace MidanMath
{
	/** Standard gravity, cm/s². Not a tuning value — changing it would make
	 *  every physics number in every Data Asset mean something different. */
	static constexpr float GravityCmS2 = 980.665f;

	/** Sea-level air density, kg/m³. Used by the aero component. Not tunable
	 *  as a design dial; the tuning dials are Cd, Cl and frontal area. */
	static constexpr float AirDensitySeaLevel = 1.225f;

	/** cm/s to km/h. */
	static constexpr float CmSToKmH = 0.036f;

	/** km/h to cm/s. */
	static constexpr float KmHToCmS = 27.7777778f;

	/** Division guarding against a zero or near-zero denominator. */
	FORCEINLINE float SafeDivide(float Numerator, float Denominator, float Fallback = 0.f)
	{
		return FMath::IsNearlyZero(Denominator) ? Fallback : (Numerator / Denominator);
	}

	/** Map In from [InMin, InMax] to [OutMin, OutMax], clamped. Degenerate
	 *  input range returns OutMin rather than dividing by zero. */
	FORCEINLINE float MapRangeClamped(float In, float InMin, float InMax, float OutMin, float OutMax)
	{
		if (FMath::IsNearlyEqual(InMin, InMax))
		{
			return OutMin;
		}
		const float Alpha = FMath::Clamp((In - InMin) / (InMax - InMin), 0.f, 1.f);
		return FMath::Lerp(OutMin, OutMax, Alpha);
	}

	/**
	 * Frame-rate-independent exponential damping.
	 *
	 * Use this instead of FMath::Lerp(Current, Target, Rate * DeltaTime).
	 * The naive Lerp form changes its effective smoothing with frame time, so
	 * a camera tuned at 60fps behaves differently at 144fps — which is a feel
	 * bug that hides behind frame-rate dependence and is miserable to find.
	 * See .claude/skills/game-feel-engineering §3.
	 *
	 * Rate is the reciprocal of the time constant: higher converges faster.
	 */
	FORCEINLINE float ExpDamp(float Current, float Target, float Rate, float DeltaTime)
	{
		if (Rate <= 0.f)
		{
			return Target;
		}
		return Target + (Current - Target) * FMath::Exp(-Rate * DeltaTime);
	}

	/** Vector form of ExpDamp. */
	FORCEINLINE FVector ExpDamp(const FVector& Current, const FVector& Target, float Rate, float DeltaTime)
	{
		if (Rate <= 0.f)
		{
			return Target;
		}
		const float Blend = FMath::Exp(-Rate * DeltaTime);
		return Target + (Current - Target) * Blend;
	}

	/**
	 * Rate-limited approach with separate rise and fall speeds, in units/sec.
	 *
	 * Asymmetric rates are the point: fast attack with slower release reads as
	 * responsive but stable, and one symmetric rate satisfies neither. Used for
	 * steering input shaping.
	 */
	FORCEINLINE float RateLimitedApproach(float Current, float Target, float RiseRate, float FallRate, float DeltaTime)
	{
		const bool bMovingAwayFromZero = FMath::Abs(Target) > FMath::Abs(Current);
		const float Rate = bMovingAwayFromZero ? RiseRate : FallRate;
		const float MaxStep = FMath::Max(Rate, 0.f) * DeltaTime;
		return FMath::Abs(Target - Current) <= MaxStep
			? Target
			: Current + FMath::Sign(Target - Current) * MaxStep;
	}
}
