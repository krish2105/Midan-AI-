#include "Tests/AILapCompletionFunctionalTest.h"

#include "MidanLapTimingSubsystem.h"

AAILapCompletionFunctionalTest::AAILapCompletionFunctionalTest()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAILapCompletionFunctionalTest::StartTest()
{
	Super::StartTest();

	ElapsedSeconds = 0.f;

	APawn* Opponent = TargetOpponent.Get();
	if (!Opponent)
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("TargetOpponent did not resolve — is it placed and possessed on the test map?"));
		return;
	}

	const UMidanLapTimingSubsystem* LapTiming = GetWorld()->GetSubsystem<UMidanLapTimingSubsystem>();
	StartingLapIndex = LapTiming ? LapTiming->GetRacerLapIndex(Opponent) : 0;
}

void AAILapCompletionFunctionalTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APawn* Opponent = TargetOpponent.Get();
	if (!Opponent)
	{
		return;
	}

	ElapsedSeconds += DeltaSeconds;

	const UMidanLapTimingSubsystem* LapTiming = GetWorld()->GetSubsystem<UMidanLapTimingSubsystem>();
	if (!LapTiming)
	{
		return;
	}

	if (LapTiming->GetRacerLapIndex(Opponent) > StartingLapIndex)
	{
		const FMidanLapRecord LastLap = LapTiming->GetRacerLastCompletedLap(Opponent);
		if (!LastLap.bValid)
		{
			FinishTest(EFunctionalTestResult::Failed, TEXT("AI completed a lap, but it was invalidated — off-track for longer than the grace period."));
			return;
		}

		FinishTest(EFunctionalTestResult::Succeeded, FString::Printf(TEXT("AI completed a valid lap in %.1fs (elapsed test time)."), ElapsedSeconds));
		return;
	}

	if (ElapsedSeconds >= TimeoutSeconds)
	{
		FinishTest(EFunctionalTestResult::Failed, FString::Printf(TEXT("AI did not complete a lap within %.1fs."), TimeoutSeconds));
	}
}
