// Functional test: an AI opponent completes at least one valid lap without
// leaving the track for longer than the grace period.
//
// Responsibility: prove AMidanOpponentController can drive a full lap in a
// real map — the racing-line follower, the speed profile, and the PID
// converge on ACTUAL geometry, not the synthetic corner sequence
// SpeedProfileSpec uses.
// Single reason to change: what "the AI completed the lap correctly" means
// changes.
//
// docs/ARCHITECTURE.md §5: "AI completes a lap without leaving track —
// Functional Test — map-based." Requires the real circuit, a real racing
// line, and a real grid — none of which a unit test can stand in for.

#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "AILapCompletionFunctionalTest.generated.h"

UCLASS()
class MIDANTESTS_API AAILapCompletionFunctionalTest : public AFunctionalTest
{
	GENERATED_BODY()

public:
	AAILapCompletionFunctionalTest();

	/** The AI-controlled vehicle already placed on the test map's grid. */
	UPROPERTY(EditInstanceOnly, Category = "Midan|Test")
	TSoftObjectPtr<APawn> TargetOpponent;

	/** Generous — this proves the AI CAN complete a lap, not that it is
	 *  fast. A tight timeout would make this a performance test wearing a
	 *  correctness test's name. */
	UPROPERTY(EditInstanceOnly, Category = "Midan|Test", meta = (ClampMin = "10.0"))
	float TimeoutSeconds = 180.f;

	//~ AFunctionalTest
	virtual void StartTest() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	int32 StartingLapIndex = 0;
	float ElapsedSeconds = 0.f;
};
