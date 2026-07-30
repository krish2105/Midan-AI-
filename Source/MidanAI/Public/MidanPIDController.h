// Generic longitudinal PID with anti-windup. Plain struct, unit-testable.
//
// Responsibility: turn a scalar error into a scalar correction.
// Single reason to change: the control law itself changes (e.g. adding
// derivative filtering).
//
// AMidanOpponentController composes TWO instances — one for throttle, one for
// brake — rather than this struct knowing about pedals at all. That split is
// what "separate throttle and brake gains" (docs/ARCHITECTURE.md §3.4) means:
// a car accelerates and decelerates under different authority (engine torque
// vs brake torque, plus drag helping one and hurting the other), so tuning
// them as one signed gain set is the wrong shape for the problem.
//
// No UWorld dependency — this is why it is a plain struct, not a component,
// and why PIDConvergenceSpec can test it directly.

#pragma once

#include "CoreMinimal.h"

struct MIDANAI_API FMidanPIDController
{
	float ProportionalGain = 0.f;
	float IntegralGain = 0.f;
	float DerivativeGain = 0.f;

	/** Anti-windup clamp on the integral accumulator, symmetric. A sustained
	 *  error (e.g. stuck behind a wall) must not let the integral term grow
	 *  without bound and then overshoot wildly once the error clears. */
	float IntegralClamp = 1.f;

	/**
	 * Advance the controller by one control-loop step and return the
	 * correction.
	 *
	 * Error is SetPoint - Measured, in whatever units the caller defines —
	 * this struct has no opinion on units, which is exactly what makes one
	 * implementation serve both throttle (speed error, km/h) and steering-
	 * adjacent uses without duplication.
	 */
	float Update(float Error, float DeltaSeconds);

	/** Clears the integral accumulator and derivative history. Call this
	 *  whenever the control loop is interrupted (e.g. a respawn) — resuming
	 *  with a stale integral term from before the interruption produces a
	 *  visible lurch. */
	void Reset();

private:
	float IntegralAccumulator = 0.f;
	float PreviousError = 0.f;
	bool bHasPreviousError = false;
};
