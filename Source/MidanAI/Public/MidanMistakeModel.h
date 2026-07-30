// Probabilistic late brake / wide line. Perfect AI is boring AI.
//
// Responsibility: decide when and how much this racer messes up.
// Single reason to change: what a "mistake" looks like changes.
//
// Plain struct — no UWorld, no component, just a random stream and a small
// amount of decaying state. Owned by AMidanOpponentController, which calls
// OnEnteredBrakingZone at the edge-trigger (bBrakingZone false -> true) and
// reads the two magnitude accessors every control update. Scaled inversely
// with difficulty through UMidanAIDifficultyDataAsset::MistakeProbability —
// this struct itself has no concept of difficulty, only of "a mistake is
// active or it isn't", which keeps it trivially testable in isolation.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

struct MIDANAI_API FMidanMistakeModel
{
	/** Seed once per racer at spawn (e.g. from the actor's unique ID) so
	 *  mistake rolls are deterministic for a given seed — useful for replay
	 *  and for reproducing a specific AI behaviour during tuning. */
	void Seed(int32 RandomSeed) { Rng = FRandomStream(RandomSeed); }

	/**
	 * Call once per braking-zone edge-trigger (the AI was not in a braking
	 * zone last update, and is now). Rolls MistakeProbability; if it hits,
	 * activates a mistake for MistakeDurationSeconds.
	 *
	 * Deliberately NOT called every tick while inside a braking zone — a
	 * mistake is a single decision made at the moment of committing to a
	 * corner, not a continuously re-rolled coin flip, which would make the
	 * chance of SOME mistake during a long braking zone approach 100%
	 * regardless of how low MistakeProbability is.
	 */
	void OnEnteredBrakingZone(float MistakeProbability, float SpeedOvershootMultiplier, float LateralOffsetCm, float DurationSeconds);

	/** Advances the decay. Call once per control update regardless of
	 *  whether a mistake is active — it is a no-op when RemainingSeconds is
	 *  already zero. */
	void Tick(float DeltaSeconds);

	bool IsActive() const { return RemainingSeconds > 0.f; }

	/** >=1 while active, linearly decaying to 1.0 over the mistake's
	 *  duration. Multiply onto the target speed to make the AI carry too
	 *  much speed into the corner — a late brake. */
	float GetSpeedOvershootMultiplier() const;

	/** Additional lateral offset, cm, decaying to 0 over the mistake's
	 *  duration. Signed randomly per-mistake (left or right) at activation. */
	float GetLateralOffsetCm() const;

private:
	FRandomStream Rng;

	float ActiveDurationSeconds = 0.f;
	float RemainingSeconds = 0.f;
	float PeakSpeedOvershootMultiplier = 1.f;
	float PeakLateralOffsetCm = 0.f;
};
