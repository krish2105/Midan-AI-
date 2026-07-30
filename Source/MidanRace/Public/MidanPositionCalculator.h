// Arc-length + lap count -> race position. Pure functions, no UWorld.
//
// Responsibility: turn per-racer progress into a finishing order.
// Single reason to change: the ranking rule changes.
//
// Free functions rather than a component, deliberately: master prompt §3.3
// requires position computed at 10Hz, not per frame, and the actual
// computation — sort by total progress — has nothing to do with a world, an
// actor, or a timer. Keeping it here means it is unit-testable without a map
// (see MidanEditorTools/PositionCalculatorSpec) and AMidanRaceGameState's 10Hz
// timer is a thin caller, not where the logic lives.

#pragma once

#include "CoreMinimal.h"

namespace MidanRacePosition
{
	/**
	 * Total progress for ordering, using FMidanTrackPosition's own convention
	 * (MidanCoreTypes.h): laps completed times track length, plus arc length
	 * into the current lap. Monotonic across the whole race, so comparing two
	 * racers is one float comparison rather than a lap-then-arc special case
	 * that gets the start/finish line wrong.
	 */
	MIDANRACE_API float ComputeTotalProgress(int32 LapIndex, float ArcLength, float TrackLength);

	/**
	 * Rank every racer by total progress, highest first.
	 *
	 * Returns one 1-based position per input element, in the SAME ORDER as
	 * ProgressPerRacer — position N corresponds to ProgressPerRacer[N]. Ties
	 * are broken by input order (the earlier index wins), which keeps the
	 * result deterministic for two racers at bit-identical progress rather
	 * than depending on sort stability the caller cannot see.
	 *
	 * Empty input returns an empty array. A single racer always returns {1}.
	 */
	MIDANRACE_API TArray<int32> ComputeRacePositions(const TArray<float>& ProgressPerRacer);
}
