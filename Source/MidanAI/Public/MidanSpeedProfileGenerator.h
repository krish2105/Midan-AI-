// Curvature -> target speed, with a backward braking pass. Pure functions,
// no UWorld.
//
// Responsibility: turn a curvature sequence into a driveable speed profile.
// Single reason to change: the speed-limiting model changes.
//
// v_target(s) = min(v_max, sqrt(mu_effective * g / |curvature(s)|),
// backward_pass_braking_limit(s)) — master prompt's formula, verbatim.
// Two passes: ComputeCorneringLimit (forward, one sample at a time, purely
// local) then ApplyBackwardBrakingPass (walks backward from each apex,
// enforcing that reaching a slow point from a faster one earlier on the lap
// is actually achievable under braking). Free functions with no UWorld
// dependency, same reasoning as MidanPositionCalculator in MidanRace: this is
// math, not behaviour, and SpeedProfileSpec exercises it directly against a
// synthetic corner sequence — the Phase 6 gate deliverable.

#pragma once

#include "CoreMinimal.h"

namespace MidanSpeedProfile
{
	/** One sample along a racing line: where it is, and how sharply it turns
	 *  there. Curvature sign is irrelevant to this module — only magnitude
	 *  feeds the speed limit — but kept signed since callers source it
	 *  straight from IMidanTrackInterface::GetCurvatureAtDistance. */
	struct MIDANAI_API FCurvatureSample
	{
		float ArcLengthCm = 0.f;
		float Curvature = 0.f; // signed, 1/cm
	};

	/**
	 * Forward pass: the pure cornering-grip speed limit at each sample,
	 * ignoring braking achievability entirely.
	 *
	 * v = min(VMaxKmh, sqrt(MuEffective * g / |curvature|)). A straight
	 * (curvature near zero) is uncapped by grip, so it clamps to VMaxKmh
	 * instead of blowing up — SafeDivide-style guarding, not a special case
	 * the caller needs to know about.
	 */
	MIDANAI_API void ComputeCorneringLimit(
		const TArray<FCurvatureSample>& Samples,
		float MuEffective,
		float VMaxKmh,
		TArray<float>& OutCorneringLimitKmh);

	/**
	 * Backward pass: enforce braking achievability walking backward around
	 * the sequence.
	 *
	 * For consecutive samples i then i+1 with exit speed already known at
	 * i+1, the fastest i may be driven while still braking to i+1's speed by
	 * the time it arrives is v_i = sqrt(v_(i+1)^2 + 2*a*d), from constant-
	 * deceleration kinematics. Runs backward because a braking POINT is
	 * determined by where you need to have SLOWED TO by, not by where you
	 * start braking from — "backward pass from each apex" per the master
	 * prompt.
	 *
	 * bClosedLoop runs two full backward passes around the sequence rather
	 * than one: a single pass starting arbitrarily on a closed loop cannot
	 * see braking requirements that wrap across the start/finish line until
	 * a second pass propagates them the rest of the way around.
	 *
	 * TrackLengthCm is only read when bClosedLoop is true, to compute the arc
	 * distance from the last sample back to the first across the finish
	 * line. Ignored for an open sequence.
	 */
	MIDANAI_API void ApplyBackwardBrakingPass(
		const TArray<FCurvatureSample>& Samples,
		const TArray<float>& CorneringLimitKmh,
		float MaxBrakingDecelerationCmS2,
		bool bClosedLoop,
		float TrackLengthCm,
		TArray<float>& OutSpeedKmh);

	/**
	 * Both passes combined, plus braking-zone flagging.
	 *
	 * OutBrakingZone[i] is true where the backward pass reduced the speed
	 * below the pure cornering limit — i.e. this point is reached under
	 * braking from something faster earlier on the lap, not merely grip-
	 * limited in isolation. This is exactly FRacingLinePoint::bBrakingZone.
	 */
	MIDANAI_API void GenerateSpeedProfile(
		const TArray<FCurvatureSample>& Samples,
		float MuEffective,
		float VMaxKmh,
		float MaxBrakingDecelerationCmS2,
		bool bClosedLoop,
		float TrackLengthCm,
		TArray<float>& OutSpeedKmh,
		TArray<bool>& OutBrakingZone);
}
