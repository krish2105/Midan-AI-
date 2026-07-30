// What race phase must expose.
//
// Responsibility: the contract letting non-Race modules read race phase.
// Single reason to change: the race state machine gains a phase.
//
// Exists so MidanAI can know not to drive during the countdown without
// depending on MidanRace (docs/ASSUMPTIONS.md A9). Declared in Phase 3 for the
// same reason as IMidanTrackInterface (A26); AMidanRaceGameState implements it
// at Phase 5.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "MidanRaceStateInterface.generated.h"

UINTERFACE(MinimalAPI)
class UMidanRaceStateInterface : public UInterface
{
	GENERATED_BODY()
};

class MIDANCORE_API IMidanRaceStateInterface
{
	GENERATED_BODY()

public:
	/** One of Race.State.Grid / Countdown / Racing / Finished / Results. */
	virtual FGameplayTag GetRaceStateTag() const = 0;

	/**
	 * True only during Race.State.Racing.
	 *
	 * A convenience the AI checks every control tick. Exists as its own method
	 * rather than a tag comparison at each call site because "may I drive" is
	 * the actual question, and encoding it once here means adding a future
	 * phase does not require auditing every caller for a missed tag.
	 */
	virtual bool IsRacingActive() const = 0;

	/** 1-based finishing order, 0 if unknown. Updated at 10Hz, not per frame. */
	virtual int32 GetRacerPosition(const AActor* Racer) const = 0;

	/** Completed laps for a racer, 0 on the opening lap. */
	virtual int32 GetRacerLapCount(const AActor* Racer) const = 0;

	/** Total racers in the field. Difficulty and rubber-band logic scale
	 *  against position within the field, so it needs the denominator. */
	virtual int32 GetRacerCount() const = 0;

	/** Seconds remaining in the countdown, 0 outside Race.State.Countdown.
	 *  Drives the HUD countdown and lets AI pre-load throttle for a launch. */
	virtual float GetCountdownRemainingSeconds() const = 0;
};
