// Grid population and Race.State phase transitions.
//
// Responsibility: get eight racers onto the grid and drive the state machine
// from Grid through Countdown, Racing, Finished, to Results.
// Single reason to change: the race-flow sequence itself changes.
//
// Never ticks — every step here is either an engine callback (PostLogin) or a
// one-shot/repeating FTimerHandle, per docs/ARCHITECTURE.md §2.3, and this
// class is deliberately absent from that tick inventory.
//
// Knows only IMidanVehicleInterface, never AMidanVehiclePawn — MidanRace does
// not depend on MidanVehicle (docs/ASSUMPTIONS.md A7). DefaultPawnClass (the
// player's car) and OpponentVehicleClasses are configured as soft class
// references on the Blueprint subclass of this GameMode, which is the
// project's one sanctioned place for a Blueprint to carry asset/class
// references without becoming gameplay logic.
//
// OpponentControllerClass defaults to plain AAIController but is intended to
// be set to AMidanOpponentController (MidanAI, Phase 6) on the Blueprint
// subclass — a TSubclassOf<AAIController> needs only AIModule (an engine
// module MidanRace already links for this exact purpose, see A32), so this
// class can drive AMidanOpponentController-possessed opponents without
// MidanRace depending on MidanAI, which the module graph forbids.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MidanRaceGameMode.generated.h"

class AAIController;
class AMidanCheckpoint;
class AMidanGridSpline;
class UMidanRaceRulesDataAsset;

UCLASS(Blueprintable)
class MIDANRACE_API AMidanRaceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMidanRaceGameMode();

	/** Structure, timing and grace numbers for this race. Soft, async-loaded —
	 *  every other Data Asset reference in this project follows the same
	 *  pattern, and a GameMode CDO is constructed at engine startup, before
	 *  any world exists to load into. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|Race")
	TSoftObjectPtr<UMidanRaceRulesDataAsset> RaceRulesAsset;

	/**
	 * Pawn class per opponent slot, cycling if shorter than the grid needs
	 * (SlotCount - 1 opponents). Master prompt fixes three vehicles; a
	 * three-entry array with each opponent driving a different car is the
	 * expected authoring, not a limitation of the cycling.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|Race")
	TArray<TSoftClassPtr<APawn>> OpponentVehicleClasses;

	/** Controller class opponents are possessed with. Defaults to plain
	 *  AAIController; set to AMidanOpponentController on the Blueprint
	 *  subclass once MidanAI is in the project. See the class comment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|Race")
	TSubclassOf<AAIController> OpponentControllerClass;

	//~ AGameModeBase
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

private:
	void OnRaceRulesLoaded();
	void GatherLevelActors();
	void RequestOpponentClassesLoad();
	void OnOpponentClassesLoaded();
	void SpawnOpponents();
	void RegisterRacer(APawn* RacerPawn, int32 GridSlotIndex);

	void TryBeginCountdown();
	void BeginCountdownSequence();
	void OnCountdownFinished();
	void HandleRacerFinished(AActor* Racer, int32 TotalLaps);
	void OnResultsDelayElapsed();

	TSharedPtr<struct FStreamableHandle> RaceRulesLoadHandle;
	TSharedPtr<struct FStreamableHandle> OpponentClassesLoadHandle;

	UPROPERTY(Transient)
	TObjectPtr<const UMidanRaceRulesDataAsset> LoadedRaceRules;

	UPROPERTY(Transient)
	TObjectPtr<AMidanGridSpline> GridSpline;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AMidanCheckpoint>> Checkpoints;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> RegisteredRacers;

	FTimerHandle CountdownTimerHandle;
	FTimerHandle ResultsDelayTimerHandle;

	bool bRaceRulesReady = false;
	bool bPlayerReady = false;
	bool bCountdownStarted = false;
	bool bRaceFinishedTriggered = false;

	/** Next unfilled grid slot for an opponent. Slot 0 is reserved for the
	 *  player. */
	int32 NextOpponentSlotIndex = 1;
};
