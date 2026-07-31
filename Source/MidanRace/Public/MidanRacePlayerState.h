// Per-racer lap, sector, position and best-time record.
//
// Responsibility: the queryable, replicable summary of one racer's race.
// Single reason to change: a new summary field is needed.
//
// A thin mirror, deliberately: UMidanLapTimingSubsystem is the source of
// truth for lap/sector progress and AMidanRaceGameState's 10Hz timer is the
// source of truth for Position. This class only listens and republishes —
// the HUD (Phase 7) reads from here rather than reaching into the subsystem,
// which is the usual GameState/PlayerState pattern and keeps the subsystem
// free of UI concerns.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MidanLapRecord.h"
#include "MidanRacePlayerState.generated.h"

UCLASS()
class MIDANRACE_API AMidanRacePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AMidanRacePlayerState();

	/** 1-based; 0 until the first position computation. */
	UPROPERTY(BlueprintReadOnly, Category = "Midan|Race")
	int32 Position = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Midan|Race")
	int32 CurrentLapIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Midan|Race")
	int32 CurrentSectorIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Midan|Race")
	bool bCurrentLapValid = true;

	UPROPERTY(BlueprintReadOnly, Category = "Midan|Race")
	FMidanLapRecord LastCompletedLap;

	UPROPERTY(BlueprintReadOnly, Category = "Midan|Race")
	float BestLapTimeSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Midan|Race")
	bool bFinishedRace = false;

	/** This sector's time minus the racer's previous best for that sector
	 *  index — negative is faster (a new best), positive is slower. Updates
	 *  once per sector completion, not continuously; see
	 *  UMidanLapTimingSubsystem::FOnMidanSectorCompleted's comment. 0 and
	 *  bHasSectorDelta false before the first sector completes. */
	UPROPERTY(BlueprintReadOnly, Category = "Midan|Race")
	float LastSectorDeltaSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Midan|Race")
	bool bHasSectorDelta = false;

	/** Called by AMidanRaceGameState's 10Hz timer. Not exposed as
	 *  BlueprintCallable — position is computed, never set by hand. */
	void SetPosition(int32 InPosition) { Position = InPosition; }

	/**
	 * Bind to this racer's UMidanLapTimingSubsystem delegates.
	 *
	 * Called once the owning pawn is possessed, since that is the earliest
	 * point the PlayerState can resolve which AActor it is tracking laps for.
	 */
	void BindToLapTiming(AActor* RacerActor);

	/** The vehicle actor this PlayerState is tracking laps for, or null before
	 *  BindToLapTiming has run. AMidanRaceGameState's position timer uses this
	 *  to map a PlayerState to a world location. */
	AActor* GetTrackedRacerActor() const { return BoundRacer.Get(); }

	//~ AActor / APlayerState
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleLapCompleted(AActor* Racer, const FMidanLapRecord& Lap);
	void HandleLapInvalidated(AActor* Racer);
	void HandleSectorCompleted(AActor* Racer, const FMidanSectorTime& Sector, float DeltaVsPreviousBestSeconds);
	void HandleRacerFinished(AActor* Racer, int32 TotalLaps);

	TWeakObjectPtr<AActor> BoundRacer;
};
