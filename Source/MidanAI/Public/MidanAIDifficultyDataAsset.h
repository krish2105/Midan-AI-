// Every tunable AI-driving value: grip confidence, reaction delay, target
// speed, mistake rate, aggression, rubber-band envelope, and the control-loop
// gains that turn racing-line samples into steering and pedal input.
//
// Responsibility: the source of truth for how one difficulty tier drives.
// Single reason to change: a new AI-tuning category is introduced.
//
// HARD ARCHITECTURAL RULE (master prompt, docs/ARCHITECTURE.md §3.4):
// difficulty modifies friction, reaction delay, target speed, mistake
// probability, and aggression. It NEVER teleports, NEVER adds engine power
// beyond the player's own car class, and rubber-banding stays inside a small
// envelope that decays to 1.0 within a few seconds. Every field below is one
// of those five levers or a control-loop gain that serves them — there is no
// "AI power multiplier" field, and there must never be one.

#pragma once

#include "CoreMinimal.h"
#include "MidanDataAsset.h"
#include "MidanAIDifficultyDataAsset.generated.h"

UCLASS(BlueprintType)
class MIDANAI_API UMidanAIDifficultyDataAsset : public UMidanDataAsset
{
	GENERATED_BODY()

public:
	// --- The five hard-rule levers -------------------------------------

	/**
	 * Cornering grip confidence, applied as sqrt(TyreFrictionMultiplier) on
	 * the reference target speed WHERE FRacingLinePoint::bBrakingZone is true.
	 * 1.0 drives the reference profile as generated; below 1.0 brakes earlier
	 * and carries less speed through corners specifically — not on straights,
	 * where TargetSpeedMultiplier is the only lever. Scaled by square root
	 * because the reference profile itself is generated from
	 * v = sqrt(mu*g/|curvature|); applying the multiplier post-sqrt keeps this
	 * field's meaning physically consistent with what generated the number
	 * it is scaling.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.3", ClampMax = "1.2"))
	float TyreFrictionMultiplier = 1.f;

	/** Seconds between a control input being computed and it reaching
	 *  ApplyInput. Modelled as a fixed-length delay buffer — see
	 *  MidanOpponentController.h — not a slower Tick rate, so the AI still
	 *  reads fresh state every frame and only its OUTPUT lags, matching how a
	 *  slower human driver actually differs from a fast one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReactionDelaySeconds = 0.15f;

	/** Uniform scalar on the reference target speed, applied everywhere
	 *  (corners and straights alike) — the primary "how fast is this
	 *  difficulty tier" dial. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.5", ClampMax = "1.05"))
	float TargetSpeedMultiplier = 0.92f;

	/** Chance, per braking-zone entry, that MidanMistakeModel triggers a
	 *  mistake at that corner. Scaled inversely with difficulty: a Hard
	 *  tier's value should be near 0, an Easy tier's noticeably above 0.
	 *  Perfect AI is boring AI — master prompt §4.3. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MistakeProbability = 0.15f;

	/** How eagerly this tier commits to an overtake — scales
	 *  UMidanOvertakeComponent's gap and closing-speed thresholds down
	 *  (lower required gap = more aggressive). 0 never attempts a pass; 1 is
	 *  the most aggressive authored behaviour. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Aggression = 0.5f;

	// --- Rubber-band envelope -------------------------------------------
	// A target-speed multiplier, same mechanism as TargetSpeedMultiplier
	// above — never a power or physics change. Bounded and self-decaying by
	// construction: UMidanRubberBandComponent cannot hold the boost, only
	// chase toward it and fall back at DecayRate.

	/** Maximum extra fraction of speed the envelope may add when this racer
	 *  is meaningfully behind the player. 0.05 = at most 5% over the
	 *  (already difficulty-scaled) target speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RubberBand", meta = (ClampMin = "0.0", ClampMax = "0.15"))
	float RubberBandMaxEnvelope = 0.05f;

	/** Exponential decay rate back toward a 1.0 multiplier, 1/seconds — fed
	 *  to MidanMath::ExpDamp. Higher decays faster. Sized so the envelope
	 *  empties within a few seconds of the gap closing, per the hard rule. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RubberBand", meta = (ClampMin = "0.2", ClampMax = "3.0"))
	float RubberBandDecayRate = 0.8f;

	/** Gap to the player, seconds of travel time, beyond which the envelope
	 *  starts chasing its maximum. Below this gap the target is 1.0 — the
	 *  envelope only ever pulls a trailing racer up, never holds a leading
	 *  one back (that would read as the player being punished for winning,
	 *  which master prompt's non-goals rule out). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RubberBand", meta = (ClampMin = "0.5"))
	float RubberBandTriggerGapSeconds = 2.5f;

	// --- Mistake model magnitude -----------------------------------------
	// MistakeProbability above is the gate; these three are what happens
	// once it fires. Kept separate so a designer can tune "how often" and
	// "how bad" independently.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mistakes", meta = (ClampMin = "1.0", ClampMax = "1.3"))
	float MistakeSpeedOvershootMultiplier = 1.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mistakes", meta = (ClampMin = "0.0", ClampMax = "400.0"))
	float MistakeLateralOffsetCm = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mistakes", meta = (ClampMin = "0.3", ClampMax = "3.0"))
	float MistakeDurationSeconds = 1.2f;

	// --- Control-loop gains ----------------------------------------------
	// How this tier translates racing-line samples into pedal and wheel
	// input. Distinct from the five hard-rule levers above: these do not
	// change WHAT the AI is trying to do, only how precisely it executes it,
	// which is itself a legitimate skill differentiator between tiers.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|Throttle")
	float ThrottleProportionalGain = 0.02f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|Throttle")
	float ThrottleIntegralGain = 0.01f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|Throttle")
	float ThrottleDerivativeGain = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|Throttle", meta = (ClampMin = "0.0"))
	float ThrottleIntegralClamp = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|Brake")
	float BrakeProportionalGain = 0.03f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|Brake")
	float BrakeIntegralGain = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|Brake")
	float BrakeDerivativeGain = 0.01f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|Brake", meta = (ClampMin = "0.0"))
	float BrakeIntegralClamp = 0.5f;

	/** Deceleration this tier assumes it can achieve under braking, cm/s² —
	 *  MidanSpeedProfileGenerator's backward-pass input when this tier's
	 *  reference profile is generated. Lower than the physical maximum on
	 *  purpose for lower tiers: it is a PLANNING assumption, never a physics
	 *  change, and produces earlier, gentler braking exactly the way a less
	 *  confident driver brakes earlier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|SpeedProfile", meta = (ClampMin = "300.0", ClampMax = "2500.0"))
	float AssumedMaxBrakingDecelerationCmS2 = 1200.f;

	/** Ceiling in the profile's min(VMax, ...) term — a generous cap, not a
	 *  vehicle top speed. The vehicle's own physics is always the binding
	 *  constraint in practice; this exists only so an uncapped straight does
	 *  not produce an unbounded sqrt(...) target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|SpeedProfile", meta = (ClampMin = "100.0", ClampMax = "500.0"))
	float TopSpeedCapKmh = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|Steering", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float SteeringResponseGain = 1.f;

	/** Max normalised steering change per second — keeps the AI's wheel input
	 *  from snapping instantly, the same reason the player's steering has
	 *  FVehicleSteeringConfig's rise/fall rates. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|Steering", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float SteeringRateLimitPerSecond = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|Steering", meta = (ClampMin = "300.0"))
	float MinLookaheadDistanceCm = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|Steering", meta = (ClampMin = "500.0"))
	float MaxLookaheadDistanceCm = 3000.f;

	/** Speed, km/h, at which lookahead distance reaches its maximum. Below
	 *  this it interpolates from MinLookaheadDistanceCm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ControlLoop|Steering", meta = (ClampMin = "50.0"))
	float LookaheadSpeedNormalisationKmh = 250.f;

	// --- Awareness (avoidance and overtaking sensing) ---------------------
	// Not the five hard-rule levers — these govern how often and how far
	// this tier looks for hazards and gaps, a legitimate skill differentiator
	// distinct from raw pace.

	/** How often UMidanAvoidanceComponent re-traces the projected path,
	 *  seconds. Lower is more alert; also the main per-tier cost knob since
	 *  each trace is the expensive part of the AI control loop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Awareness", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float AvoidanceTraceIntervalSeconds = 0.15f;

	/** Furthest this tier will dodge sideways to avoid a predicted hazard,
	 *  cm, before the only remaining option is braking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Awareness", meta = (ClampMin = "50.0", ClampMax = "500.0"))
	float AvoidanceMaxOffsetCm = 250.f;

	/** How often UMidanOvertakeComponent checks for a pass opportunity,
	 *  seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Awareness", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float OvertakeCheckIntervalSeconds = 0.5f;

	/** Minimum gap to a car ahead, seconds of travel time at closing speed,
	 *  before considering a pass. Scaled down by Aggression at runtime — see
	 *  UMidanOvertakeComponent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Awareness", meta = (ClampMin = "0.3", ClampMax = "5.0"))
	float OvertakeMinGapSeconds = 1.5f;

	/** How far ahead UMidanOvertakeComponent looks for a car to pass, cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Awareness", meta = (ClampMin = "500.0", ClampMax = "8000.0"))
	float OvertakeDetectionRangeCm = 3000.f;

	//~ UMidanDataAsset
	virtual void ValidateMidanData(FMidanValidationResult& Result) const override;
};
