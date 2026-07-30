// Every tunable race-flow number: lap count, countdown, off-track grace,
// respawn cooldown, sector count, position update rate.
//
// Responsibility: the source of truth for how a race is structured.
// Single reason to change: a new race-flow parameter is introduced.
//
// CLAUDE.md forbids hardcoded tuning values, and "how many laps" is exactly
// that — a design dial, not a constant. AMidanRaceGameMode, AMidanRaceGameState
// and UMidanLapTimingSubsystem all read from one instance of this asset rather
// than each holding its own copy of, say, OffTrackGraceSeconds.

#pragma once

#include "CoreMinimal.h"
#include "MidanDataAsset.h"
#include "MidanLapRecord.h"
#include "MidanRaceRulesDataAsset.generated.h"

UCLASS(BlueprintType)
class MIDANRACE_API UMidanRaceRulesDataAsset : public UMidanDataAsset
{
	GENERATED_BODY()

public:
	/** Laps to complete. Assumption A13 in docs/ASSUMPTIONS.md: never specified
	 *  upstream, so this is a tunable starting point, not a design decision
	 *  made in code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Race", meta = (ClampMin = "1"))
	int32 LapCount = 3;

	/** Seconds from Race.State.Grid entering Countdown to the lights going out. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Race", meta = (ClampMin = "0.0"))
	float CountdownDurationSeconds = 3.f;

	/** Seconds AMidanRaceGameMode holds Race.State.Finished before advancing to
	 *  Race.State.Results. A pacing value, not a technical one — long enough
	 *  for a finish-line moment to read, short enough not to feel stuck. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Race", meta = (ClampMin = "0.0"))
	float ResultsDelaySeconds = 3.f;

	/** How many sectors a lap is split into. Clamped to
	 *  MidanRaceConstants::MaxSectorCount because FMidanLapRecord::Sectors is a
	 *  fixed-size array. Master prompt §3.2 fixes this at 3. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Race",
		meta = (ClampMin = "1", ClampMax = "4"))
	int32 SectorCount = 3;

	/**
	 * Continuous seconds off-track before the current lap invalidates.
	 *
	 * A grace period rather than instant invalidation because a two-wheel kerb
	 * clip is normal racing, not a cut. Master prompt requires physical-material
	 * sampling with a grace threshold, not trigger-volume instant invalidation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Race", meta = (ClampMin = "0.1"))
	float OffTrackGraceSeconds = 1.5f;

	/** Wheels that must be off-track simultaneously to accrue grace time.
	 *  Below this, e.g. one wheel clipping a kerb apex, does not count at all —
	 *  it is not merely graced, it is not off-track. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Race",
		meta = (ClampMin = "1", ClampMax = "4"))
	int32 OffTrackWheelThreshold = 2;

	/** How often UMidanLapTimingSubsystem polls each racer's off-track wheel
	 *  count. A timer, not a per-frame Tick — see docs/ARCHITECTURE.md §2.3.
	 *  Must be well below OffTrackGraceSeconds or the grace timer becomes
	 *  quantised into visibly unfair chunks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Race",
		meta = (ClampMin = "0.02", ClampMax = "1.0"))
	float OffTrackPollIntervalSeconds = 0.1f;

	/** Seconds a respawned vehicle is invulnerable to a second respawn request.
	 *  Prevents a player who respawns onto more off-track ground from chaining
	 *  requests, and prevents a stuck-and-mashing player from cycling forever. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Race", meta = (ClampMin = "0.5"))
	float RespawnCooldownSeconds = 5.f;

	/** Rate AMidanRaceGameState recomputes race position, Hz. Master prompt §3.3
	 *  fixes this at 10Hz, explicitly not per frame — see docs/ARCHITECTURE.md
	 *  §2.3. Exposed as data rather than a constant because a profiling pass may
	 *  need it lower on a struggling platform. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Race",
		meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float PositionUpdateHz = 10.f;

	//~ UMidanDataAsset
	virtual void ValidateMidanData(FMidanValidationResult& Result) const override;
};
