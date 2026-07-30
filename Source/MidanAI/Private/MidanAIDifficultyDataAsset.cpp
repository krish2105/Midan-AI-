#include "MidanAIDifficultyDataAsset.h"

#define LOCTEXT_NAMESPACE "MidanAIDifficultyDataAsset"

void UMidanAIDifficultyDataAsset::ValidateMidanData(FMidanValidationResult& Result) const
{
	if (TyreFrictionMultiplier <= 0.f)
	{
		Result.AddError(TEXT("TyreFrictionMultiplier"),
			LOCTEXT("BadFriction", "Tyre friction multiplier must be positive — it appears under a square root."));
	}

	if (TargetSpeedMultiplier <= 0.f)
	{
		Result.AddError(TEXT("TargetSpeedMultiplier"),
			LOCTEXT("BadTargetSpeed", "Target speed multiplier must be positive."));
	}
	else if (TargetSpeedMultiplier > 1.0f)
	{
		Result.AddWarning(TEXT("TargetSpeedMultiplier"),
			LOCTEXT("TargetSpeedAboveReference", "Above 1.0: this tier drives faster than the reference profile. Confirm that is intentional for a Hard/Expert tier — it should never need to exceed what the reference profile itself assumes is achievable."));
	}

	if (MistakeProbability > 0.f && ReactionDelaySeconds <= 0.f)
	{
		Result.AddWarning(TEXT("ReactionDelaySeconds"),
			LOCTEXT("ZeroReactionWithMistakes", "Reaction delay is zero on a tier that still makes mistakes. Usually a Hard/Expert tier pairs a low mistake probability with a low but non-zero reaction delay, not zero."));
	}

	if (RubberBandMaxEnvelope > 0.f && RubberBandDecayRate <= 0.f)
	{
		Result.AddError(TEXT("RubberBandDecayRate"),
			LOCTEXT("NoDecay", "Rubber-band decay rate must be positive whenever the envelope is non-zero, or a boost never returns to 1.0 — violating the hard rule that rubber-banding stays inside a small envelope that decays."));
	}

	if (AssumedMaxBrakingDecelerationCmS2 <= 0.f)
	{
		Result.AddError(TEXT("AssumedMaxBrakingDecelerationCmS2"),
			LOCTEXT("BadDecel", "Assumed braking deceleration must be positive — it is a denominator-adjacent term in the backward braking pass."));
	}

	if (MinLookaheadDistanceCm >= MaxLookaheadDistanceCm)
	{
		Result.AddError(TEXT("MinLookaheadDistanceCm"),
			LOCTEXT("BadLookaheadRange", "MinLookaheadDistanceCm must be less than MaxLookaheadDistanceCm."));
	}

	if (ThrottleIntegralGain > 0.f && ThrottleIntegralClamp <= 0.f)
	{
		Result.AddError(TEXT("ThrottleIntegralClamp"),
			LOCTEXT("NoThrottleClamp", "Integral clamp must be positive whenever the integral gain is non-zero, or the anti-windup guard does nothing."));
	}

	if (BrakeIntegralGain > 0.f && BrakeIntegralClamp <= 0.f)
	{
		Result.AddError(TEXT("BrakeIntegralClamp"),
			LOCTEXT("NoBrakeClamp", "Integral clamp must be positive whenever the integral gain is non-zero, or the anti-windup guard does nothing."));
	}

	if (AvoidanceTraceIntervalSeconds <= 0.f)
	{
		Result.AddError(TEXT("AvoidanceTraceIntervalSeconds"),
			LOCTEXT("BadAvoidanceInterval", "Avoidance trace interval must be positive."));
	}

	if (OvertakeCheckIntervalSeconds <= 0.f)
	{
		Result.AddError(TEXT("OvertakeCheckIntervalSeconds"),
			LOCTEXT("BadOvertakeInterval", "Overtake check interval must be positive."));
	}
}

#undef LOCTEXT_NAMESPACE
