// Race phase state machine and 10Hz position computation.
//
// Responsibility: what phase the race is in, and each racer's finishing order.
// Single reason to change: the state machine gains a phase, or how position is
// computed changes.
//
// Implements IMidanRaceStateInterface so MidanAI (Phase 6) can read race phase
// without depending on MidanRace — see docs/ASSUMPTIONS.md A9. Position is
// computed on a timer at UMidanRaceRulesDataAsset::PositionUpdateHz (10Hz by
// default), explicitly not per frame — master prompt §3.3, docs/ARCHITECTURE.md
// §2.3. Countdown remaining is computed on demand from a stamped end time
// rather than decremented every frame, the same pattern as
// UMidanRespawnComponent's cooldown — see that class for the reasoning.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MidanRaceStateInterface.h"
#include "MidanRaceGameState.generated.h"

class UMidanRaceRulesDataAsset;

UCLASS()
class MIDANRACE_API AMidanRaceGameState : public AGameStateBase, public IMidanRaceStateInterface
{
	GENERATED_BODY()

public:
	AMidanRaceGameState();

	/** Called once by AMidanRaceGameMode after RaceRules has loaded. Starts the
	 *  position timer; safe to call before racers exist. */
	void InitialiseRaceRules(const UMidanRaceRulesDataAsset* InRaceRules);

	/** Moves the state machine. AMidanRaceGameMode is the only caller —
	 *  transitions are a game-mode decision, this class just holds and
	 *  publishes the current one. */
	void SetRaceStateTag(FGameplayTag NewState);

	/** Stamps the countdown end time so GetCountdownRemainingSeconds can
	 *  compute on demand. Does not itself change the state tag — the caller
	 *  is expected to also call SetRaceStateTag(Race.State.Countdown). */
	void BeginCountdown(float DurationSeconds);

	/** Total elapsed race time since Race.State.Racing began, seconds. 0
	 *  before racing starts. Frozen once Race.State.Finished is entered. */
	UFUNCTION(BlueprintPure, Category = "Midan|Race")
	float GetRaceElapsedSeconds() const;

	void MarkRaceStarted();
	void MarkRaceFinished();

	//~ IMidanRaceStateInterface
	virtual FGameplayTag GetRaceStateTag() const override { return CurrentStateTag; }
	virtual bool IsRacingActive() const override;
	virtual int32 GetRacerPosition(const AActor* Racer) const override;
	virtual int32 GetRacerLapCount(const AActor* Racer) const override;
	virtual int32 GetRacerSectorIndex(const AActor* Racer) const override;
	virtual int32 GetRacerCount() const override;
	virtual float GetCountdownRemainingSeconds() const override;

	//~ AActor
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RecomputePositions();

	UPROPERTY(Transient)
	FGameplayTag CurrentStateTag;

	UPROPERTY(Transient)
	TObjectPtr<const UMidanRaceRulesDataAsset> RaceRules;

	FTimerHandle PositionUpdateTimerHandle;

	float CountdownEndsAtWorldTimeSeconds = -1.f;
	float RaceStartWorldTimeSeconds = -1.f;
	float RaceElapsedSecondsAtFinish = 0.f;
	bool bRaceFinished = false;
};
