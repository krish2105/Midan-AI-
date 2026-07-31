// Persisted race records: best lap and sector times per vehicle.
//
// Responsibility: what survives between sessions.
// Single reason to change: a new persisted record is needed.
//
// Lives in MidanCore, not MidanRace, so MidanRace's own lap/sector types
// (FMidanLapRecord, FMidanSectorTime) aren't what gets saved — a save file
// format coupled to MidanRace's live gameplay structs would break the moment
// either changes shape for an unrelated reason. This struct is deliberately
// its own flat, minimal record, one intentional divergence for a
// forward-compatible file format over the small duplication cost.
//
// A single-circuit vertical slice needs one save slot's worth of records,
// keyed by vehicle rather than by track — there IS only one track. A future
// multi-track build would key by (Track, Vehicle) instead; that reshape is
// listed here so it is not a surprise.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "MidanSaveGame.generated.h"

namespace MidanSaveConstants
{
	/**
	 * Mirrors MidanRace's MidanRaceConstants::MaxSectorCount — deliberately a
	 * DIFFERENT namespace, not a reopening of MidanRace's. MidanCore depends
	 * on nothing project-side and MidanRace depends on MidanCore, so any
	 * translation unit in MidanRace (or later modules) could plausibly
	 * include both MidanLapRecord.h and MidanSaveGame.h; reusing the same
	 * namespace and constant name would redefine the same symbol twice in
	 * one TU. The VALUE is duplicated for the structural reason given below;
	 * the NAME is kept distinct so the duplication cannot become a compile
	 * error somewhere downstream.
	 */
	static constexpr int32 MaxSectorCount = 4;
}

/** Best times for one vehicle. */
USTRUCT(BlueprintType)
struct MIDANCORE_API FMidanVehicleRecord
{
	GENERATED_BODY()

	/** Matches UVehicleSetupDataAsset's asset name — a soft path string
	 *  rather than a TSoftObjectPtr, since a save file must remain loadable
	 *  even if the vehicle asset is later moved or renamed; MidanSaveGame
	 *  degrades to "no record found" rather than a broken reference. */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	FString VehicleAssetName;

	UPROPERTY(BlueprintReadOnly, Category = "Record")
	float BestLapTimeSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Record")
	float BestSectorTimeSeconds[MidanSaveConstants::MaxSectorCount] = {};

	/** True for each sector index actually recorded — a fixed array always
	 *  has all four slots, but a vehicle may not have completed enough laps
	 *  to have set every sector's best yet. */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	bool bSectorRecorded[MidanSaveConstants::MaxSectorCount] = {};
};

UCLASS()
class MIDANCORE_API UMidanSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Slot name every save/load call uses. A named constant rather than a
	 *  string literal at each call site, per CLAUDE.md's tag/string-literal
	 *  discipline applied to save slots too. */
	static const FString& GetSaveSlotName();

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	TArray<FMidanVehicleRecord> VehicleRecords;

	/** Finds or creates the record for a vehicle. Never null — a fresh
	 *  vehicle gets a fresh zeroed record rather than the caller needing a
	 *  null check on every read. */
	FMidanVehicleRecord& FindOrAddRecord(const FString& VehicleAssetName);

	const FMidanVehicleRecord* FindRecord(const FString& VehicleAssetName) const;

	/** Updates BestLapTimeSeconds if faster; returns true if it changed. */
	bool SubmitLapTime(const FString& VehicleAssetName, float LapTimeSeconds);

	/** Updates BestSectorTimeSeconds[SectorIndex] if faster; returns true if
	 *  it changed. Out-of-range SectorIndex is a no-op returning false. */
	bool SubmitSectorTime(const FString& VehicleAssetName, int32 SectorIndex, float SectorTimeSeconds);
};
