// Functional test: a full race — grid, countdown, racing, finish — reaches
// Race.State.Results and produces standings.
//
// Responsibility: prove AMidanRaceGameMode's whole state machine runs to
// completion in a real map.
// Single reason to change: what "the race completed correctly" means changes.
//
// docs/ARCHITECTURE.md §5: "Race completes and produces results — Functional
// Test — map-based." This is the integration test every other functional
// test in this module is a narrower slice of — it exercises grid population,
// the countdown, lap timing, AI driving, and the results transition
// together, on a test map configured with a very short LapCount (1) so a
// full race is inexpensive to run repeatedly in CI.

#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "RaceCompletionFunctionalTest.generated.h"

UCLASS()
class MIDANTESTS_API ARaceCompletionFunctionalTest : public AFunctionalTest
{
	GENERATED_BODY()

public:
	ARaceCompletionFunctionalTest();

	/** Generous: a 1-lap race with 7 AI opponents finishing (or the player
	 *  finishing, per AMidanRaceGameMode's finish rule) should complete well
	 *  inside this on the short test-map circuit this runs against. */
	UPROPERTY(EditInstanceOnly, Category = "Midan|Test", meta = (ClampMin = "30.0"))
	float TimeoutSeconds = 300.f;

	//~ AFunctionalTest
	virtual void StartTest() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	float ElapsedSeconds = 0.f;
};
