// Bounded speed-multiplier envelope, decaying to 1.0. Logs every application.
//
// Responsibility: give a trailing racer a small, self-limiting pace boost.
// Single reason to change: the catch-up trigger or envelope shape changes.
//
// HARD RULE (docs/ARCHITECTURE.md §3.4): this is a TARGET-SPEED multiplier,
// the exact same mechanism as UMidanAIDifficultyDataAsset::TargetSpeedMultiplier
// — never a power, torque, or physics change, and the envelope is bounded and
// self-decaying by construction (UpdateEnvelope can only chase toward
// RubberBandMaxEnvelope and always decays back). Only ever boosts a racer
// meaningfully BEHIND the player — a leading racer never gets held back,
// which would read as the game punishing the player for winning.
//
// Every application is logged to IMidanTelemetrySink so the effect is
// provable rather than a black box (master prompt §4.4) — logged on STATE
// CHANGE (crossing into/out of an active boost), not every tick, so the log
// is a readable timeline rather than noise.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MidanRubberBandComponent.generated.h"

class UMidanAIDifficultyDataAsset;

UCLASS(ClassGroup = (Midan))
class MIDANAI_API UMidanRubberBandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMidanRubberBandComponent();

	void Initialise(const UMidanAIDifficultyDataAsset* Difficulty);

	/** >=1.0. Multiply onto the (already difficulty-scaled) target speed. */
	float GetCurrentMultiplier() const { return CurrentMultiplier; }

	/**
	 * Advance the envelope by one control-loop step.
	 *
	 * Called from AMidanOpponentController's own per-frame Tick rather than
	 * ticking independently — this component does no expensive work (no
	 * traces, no queries), so it rides the one ticking system
	 * docs/ARCHITECTURE.md §2.3 already accounts for instead of adding a
	 * second tick entry for arithmetic this cheap.
	 */
	void UpdateEnvelope(float DeltaTime);

private:
	/** Gap to the player, seconds of travel time at this racer's own current
	 *  speed. Positive means the player is further along than this racer
	 *  (this racer is behind). Computed here rather than sourced from
	 *  MidanRace's position calculator: MidanAI does not depend on MidanRace
	 *  (docs/ASSUMPTIONS.md A9), and the one-line total-progress formula
	 *  (lap*trackLength + arcLength) is cheap enough to duplicate rather than
	 *  worth a module dependency for. */
	float ComputeGapToPlayerSeconds() const;

	float MaxEnvelope = 0.05f;
	float DecayRate = 0.8f;
	float TriggerGapSeconds = 2.5f;

	float CurrentMultiplier = 1.f;
	bool bWasBoosting = false;
};
