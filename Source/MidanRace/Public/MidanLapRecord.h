// Lap and sector time records.
//
// Responsibility: the completed-time data contract between UMidanLapTimingSubsystem
// and everything that displays or stores it (HUD at Phase 7, telemetry at Phase 9).
// Single reason to change: a new time field needs recording.
//
// Plain PODs, no UWorld dependency — same reasoning as FMidanTrackPosition in
// MidanCore: a lap record is a value, not a behaviour.

#pragma once

#include "CoreMinimal.h"
#include "MidanLapRecord.generated.h"

namespace MidanRaceConstants
{
	/**
	 * Upper bound on authored sector count, fixed at four.
	 *
	 * Structural, not tuning, under the same CLAUDE.md exception as
	 * MidanVehicleConstants::NumWheels: FMidanLapRecord::Sectors is a
	 * fixed-size array because a lap record is copied on the game thread far
	 * more often than it is written (HUD, telemetry, results), and a TArray
	 * would allocate on every copy. UMidanRaceRulesDataAsset::SectorCount is
	 * still the tunable value; it is clamped to this ceiling.
	 */
	static constexpr int32 MaxSectorCount = 4;
}

/** One sector split, cm/s clock. */
USTRUCT(BlueprintType)
struct MIDANRACE_API FMidanSectorTime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	int32 SectorIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float TimeSeconds = 0.f;

	/** False when the racer was off-track for any part of this sector for
	 *  longer than the grace period — the sector still has a time, but it
	 *  cannot count toward a best-sector record. */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	bool bValid = true;
};

/** One completed lap. */
USTRUCT(BlueprintType)
struct MIDANRACE_API FMidanLapRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	int32 LapIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float TotalTimeSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	FMidanSectorTime Sectors[MidanRaceConstants::MaxSectorCount];

	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	int32 SectorCount = 0;

	/** False when any wheel exceeded the off-track grace period during the lap,
	 *  or the checkpoint sequence was broken (corner-cutting, reverse driving).
	 *  An invalid lap still completes and still costs the racer the time — it
	 *  just cannot set a best lap or count toward a fastest-lap award. */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	bool bValid = true;

	/** Total elapsed race time at the moment this lap was completed. Distinct
	 *  from TotalTimeSeconds (this lap's own duration) — the results table
	 *  needs both. */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float RaceTimeAtCompletionSeconds = 0.f;
};
