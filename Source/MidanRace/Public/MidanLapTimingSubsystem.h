// Checkpoint sequencing, sector and lap timing, off-track invalidation.
//
// Responsibility: turn raw checkpoint crossings and wheel surface state into
// validated laps.
// Single reason to change: the lap-validity rule changes.
//
// The sequencing and grace-accumulation RULES are pure static functions
// (EvaluateCheckpointCross, AccumulateOffTrackGrace) so an Automation Spec can
// assert the corner-cutting and reverse-direction rejection cases without a
// world, a checkpoint actor, or a vehicle — see
// Private/Tests/LapValidationSpec.cpp and docs/ARCHITECTURE.md §5. The
// UWorldSubsystem around them is the thin, event-driven wiring: checkpoint
// overlap delegates and an off-track poll timer, per docs/ARCHITECTURE.md §2.3
// — explicitly not a per-frame Tick.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MidanLapRecord.h"
#include "MidanLapTimingSubsystem.generated.h"

class AMidanCheckpoint;
class UMidanRaceRulesDataAsset;

/**
 * Result of matching one checkpoint crossing against a racer's expected
 * sequence position.
 *
 * Deliberately only two non-ignored outcomes. Sector completion is a
 * SEPARATE, orthogonal question — "did the crossed checkpoint's SectorIndex
 * differ from the racer's current sector" — answered by the caller against
 * live checkpoint data, because the pure sequence rule below has no sector
 * metadata to reason about. Combining the two into one outcome would make
 * the pure function no longer pure (it would need a checkpoint reference)
 * for no benefit: the caller already has both pieces of information at the
 * point it needs to act on either.
 */
UENUM(BlueprintType)
enum class EMidanCheckpointCrossOutcome : uint8
{
	/** Crossed index was not the next expected one: out-of-order (corner-cut
	 *  or reverse-direction) crossing. No state changes. */
	Ignored,

	/** Advanced the sequence; the lap is not yet complete. */
	Progressed,

	/** Advanced the sequence AND wrapped back to checkpoint 0 — lap complete. */
	LapCompleted
};

USTRUCT()
struct MIDANRACE_API FMidanCheckpointCrossResult
{
	GENERATED_BODY()

	UPROPERTY()
	EMidanCheckpointCrossOutcome Outcome = EMidanCheckpointCrossOutcome::Ignored;

	/** Racer's NextExpectedCheckpointIndex after this evaluation. Unchanged
	 *  from the input when Outcome is Ignored. */
	UPROPERTY()
	int32 NewNextExpectedIndex = 0;
};

/**
 * Per-racer progress, kept internal to the subsystem. Exposed as a value type
 * so tests can construct one directly without a live racer or a world.
 */
USTRUCT()
struct MIDANRACE_API FMidanRacerLapProgress
{
	GENERATED_BODY()

	int32 NextExpectedCheckpointIndex = 1;
	int32 CurrentLapIndex = 0;
	int32 CurrentSectorIndex = 0;
	float LapStartRaceTimeSeconds = 0.f;
	float SectorStartRaceTimeSeconds = 0.f;
	float OffTrackGraceTimerSeconds = 0.f;
	bool bCurrentLapValid = true;
	bool bFinishedRace = false;

	/** Arc length of the last checkpoint accepted into the sequence (not
	 *  ignored as out-of-order). UMidanRespawnComponent snaps back to this —
	 *  a known-safe waypoint, rather than the nearest point on the centreline
	 *  to wherever the racer currently is, which could be right back at the
	 *  hazard it just left. */
	float LastValidCheckpointArcLength = 0.f;

	TArray<FMidanSectorTime> SectorsThisLap;

	float BestLapTimeSeconds = TNumericLimits<float>::Max();
	float BestSectorTimeSeconds[MidanRaceConstants::MaxSectorCount] = {};
	FMidanLapRecord LastCompletedLap;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMidanLapCompleted, AActor* /*Racer*/, const FMidanLapRecord& /*Lap*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMidanLapInvalidated, AActor* /*Racer*/);
/** DeltaVsPreviousBestSeconds: this sector's time minus whatever the racer's
 *  best for this sector index was BEFORE this crossing — 0 on a racer's
 *  first time through a sector, when there is no previous best to compare
 *  against. Computed here rather than left for a listener to derive, because
 *  by the time the broadcast fires the subsystem's own best-sector record
 *  may already have been updated to THIS time (if it's a new best), which
 *  would make "vs previous best" unrecoverable downstream. Phase 7's HUD
 *  sector-delta display (ART_DIRECTION §8.1) is the reason this exists. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMidanSectorCompleted, AActor* /*Racer*/, const FMidanSectorTime& /*Sector*/, float /*DeltaVsPreviousBestSeconds*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMidanRacerFinished, AActor* /*Racer*/, int32 /*TotalLaps*/);

UCLASS()
class MIDANRACE_API UMidanLapTimingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// -------------------------------------------------------------------
	// Pure rules — no UWorld, no UObject. This is what the Automation Specs
	// exercise directly.
	// -------------------------------------------------------------------

	/**
	 * Evaluate one checkpoint crossing against a racer's expected sequence
	 * position.
	 *
	 * Strict-sequence matching is the entire rejection mechanism: a crossing
	 * only registers when CrossedCheckpointIndex exactly equals
	 * NextExpectedIndex. Skipping ahead (corner-cutting a chicane to hit
	 * checkpoint 5 without 3 and 4) never matches the expected index, and
	 * driving backward (crossing checkpoints in descending order) never
	 * matches it either — both fall through to Ignored via the same rule,
	 * with no separate "is this reversed" check needed.
	 *
	 * CheckpointCount must be the total number of checkpoints, including
	 * index 0 (the finish line). A lap completes when NextExpectedIndex has
	 * wrapped to 0 (every intermediate checkpoint reached in order) and
	 * checkpoint 0 is crossed again.
	 */
	static FMidanCheckpointCrossResult EvaluateCheckpointCross(
		int32 NextExpectedIndex,
		int32 CrossedCheckpointIndex,
		int32 CheckpointCount);

	/**
	 * Advance (or reset) the continuous off-track grace timer for one poll
	 * tick.
	 *
	 * bIsOffTrack is the poll-time sample (OffTrackWheelThreshold or more
	 * wheels off the racing surface). The timer accumulates while off-track
	 * and resets to zero the instant the sample comes back clean — a grace
	 * PERIOD, not a grace BUDGET, so recovering before the limit costs
	 * nothing on the next excursion. Returns the new timer value; sets
	 * bOutInvalidated when this tick crossed GraceLimitSeconds.
	 */
	static float AccumulateOffTrackGrace(
		float CurrentGraceTimerSeconds,
		bool bIsOffTrack,
		float DeltaSeconds,
		float GraceLimitSeconds,
		bool& bOutInvalidated);

	// -------------------------------------------------------------------
	// World-scoped wiring
	// -------------------------------------------------------------------

	/** Called once by AMidanRaceGameMode after RaceRules has loaded and every
	 *  AMidanCheckpoint in the level has begun play. Sorts and validates the
	 *  checkpoint sequence (contiguous 0..N-1, no gaps or duplicates) and
	 *  binds each checkpoint's crossing delegate. */
	void InitialiseForRace(const UMidanRaceRulesDataAsset* InRaceRules, const TArray<AMidanCheckpoint*>& InCheckpoints);

	/** Adds a racer to lap tracking and resets its progress to the start line.
	 *  Called by AMidanRaceGameMode when populating the grid. */
	void RegisterRacer(AActor* Racer);
	void UnregisterRacer(AActor* Racer);

	/** Called by AMidanRaceGameMode when Race.State.Countdown ends. Resets
	 *  every registered racer's lap/sector clock to now and starts the
	 *  off-track poll timer. */
	void StartRace();

	/** Called by AMidanRaceGameMode on teardown. Stops the poll timer. */
	void StopRace();

	int32 GetRacerLapIndex(const AActor* Racer) const;
	int32 GetRacerSectorIndex(const AActor* Racer) const;
	bool IsRacerLapValid(const AActor* Racer) const;
	bool IsRacerFinished(const AActor* Racer) const;
	float GetRacerBestLapTimeSeconds(const AActor* Racer) const;
	FMidanLapRecord GetRacerLastCompletedLap(const AActor* Racer) const;

	/** Arc length of the last checkpoint this racer validly crossed. 0 (the
	 *  start line) for a racer who has not crossed one yet. Read by
	 *  UMidanRespawnComponent. */
	float GetRacerLastCheckpointArcLength(const AActor* Racer) const;

	FOnMidanLapCompleted OnLapCompleted;
	FOnMidanLapInvalidated OnLapInvalidated;
	FOnMidanSectorCompleted OnSectorCompleted;
	FOnMidanRacerFinished OnRacerFinished;

	//~ UWorldSubsystem
	virtual void Deinitialize() override;

private:
	void HandleCheckpointCrossed(AMidanCheckpoint* Checkpoint, AActor* Racer);
	void PollOffTrackState();

	FMidanRacerLapProgress* FindProgress(const AActor* Racer);
	const FMidanRacerLapProgress* FindProgress(const AActor* Racer) const;

	UPROPERTY(Transient)
	TObjectPtr<const UMidanRaceRulesDataAsset> RaceRules;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AMidanCheckpoint>> SortedCheckpoints;

	TMap<TWeakObjectPtr<AActor>, FMidanRacerLapProgress> RacerProgress;

	FTimerHandle OffTrackPollTimerHandle;

	bool bRaceRunning = false;
};
