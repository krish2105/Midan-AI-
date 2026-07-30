// Automation Specs for the pure checkpoint-sequencing and off-track-grace
// rules in UMidanLapTimingSubsystem.
//
// Both rules are static functions with no UWorld, no checkpoint actor, and no
// vehicle — see the class comment in MidanLapTimingSubsystem.h for why. These
// tests are the Phase 5 gate deliverable: "run the lap-validation Automation
// Specs... including the corner-cutting and reverse-direction rejection cases."

#include "Misc/AutomationTest.h"
#include "MidanLapTimingSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 TestCheckpointCount = 6; // indices 0..5, 0 is the finish line.
}

// ---------------------------------------------------------------------------
// Sequential progress — the happy path every other test is contrasted against.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanLapSequentialProgressTest,
	"Midan.Race.LapTiming.SequentialProgressAdvances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanLapSequentialProgressTest::RunTest(const FString& Parameters)
{
	int32 NextExpected = 1;

	for (int32 Checkpoint = 1; Checkpoint < TestCheckpointCount; ++Checkpoint)
	{
		const FMidanCheckpointCrossResult Result =
			UMidanLapTimingSubsystem::EvaluateCheckpointCross(NextExpected, Checkpoint, TestCheckpointCount);

		TestTrue(TEXT("In-order checkpoint progresses"), Result.Outcome == EMidanCheckpointCrossOutcome::Progressed);
		TestEqual(TEXT("Next expected advances by one"), Result.NewNextExpectedIndex, Checkpoint + 1);
		NextExpected = Result.NewNextExpectedIndex;
	}

	// NextExpected has wrapped to 0 — crossing the finish line now completes the lap.
	TestEqual(TEXT("Sequence wrapped to checkpoint 0 after the last intermediate checkpoint"), NextExpected, 0);

	const FMidanCheckpointCrossResult FinishResult =
		UMidanLapTimingSubsystem::EvaluateCheckpointCross(NextExpected, 0, TestCheckpointCount);

	TestTrue(TEXT("Crossing the finish line after every checkpoint completes the lap"), FinishResult.Outcome == EMidanCheckpointCrossOutcome::LapCompleted);
	TestEqual(TEXT("Next expected resets to 1 for the following lap"), FinishResult.NewNextExpectedIndex, 1);

	return true;
}

// ---------------------------------------------------------------------------
// Corner-cutting rejection — skipping ahead in the sequence.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanLapCornerCuttingRejectedTest,
	"Midan.Race.LapTiming.CornerCuttingIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanLapCornerCuttingRejectedTest::RunTest(const FString& Parameters)
{
	// Racer has crossed checkpoint 1 and 2; expects checkpoint 3 next. Cutting
	// across the infield to checkpoint 5 must not register as progress.
	const int32 NextExpected = 3;

	const FMidanCheckpointCrossResult Result =
		UMidanLapTimingSubsystem::EvaluateCheckpointCross(NextExpected, 5, TestCheckpointCount);

	TestTrue(TEXT("Crossing a checkpoint ahead of the expected one is ignored"), Result.Outcome == EMidanCheckpointCrossOutcome::Ignored);
	TestEqual(TEXT("Next expected is unchanged by an ignored crossing"), Result.NewNextExpectedIndex, NextExpected);

	// The racer then legitimately reaches checkpoint 3 — sequence resumes
	// exactly as if the cut attempt never happened.
	const FMidanCheckpointCrossResult Recovery =
		UMidanLapTimingSubsystem::EvaluateCheckpointCross(NextExpected, 3, TestCheckpointCount);

	TestTrue(TEXT("The correct next checkpoint still progresses normally after a rejected cut"), Recovery.Outcome == EMidanCheckpointCrossOutcome::Progressed);
	TestEqual(TEXT("Next expected advances from the un-corrupted state"), Recovery.NewNextExpectedIndex, 4);

	return true;
}

// ---------------------------------------------------------------------------
// Reverse-direction rejection — driving the course backward.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanLapReverseDirectionRejectedTest,
	"Midan.Race.LapTiming.ReverseDirectionIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanLapReverseDirectionRejectedTest::RunTest(const FString& Parameters)
{
	// Racer expects checkpoint 4. Reversing back over checkpoint 3 (already
	// accepted) must not register, and must not move the sequence backward.
	const int32 NextExpected = 4;

	const FMidanCheckpointCrossResult Result =
		UMidanLapTimingSubsystem::EvaluateCheckpointCross(NextExpected, 3, TestCheckpointCount);

	TestTrue(TEXT("Re-crossing a checkpoint already behind the racer is ignored"), Result.Outcome == EMidanCheckpointCrossOutcome::Ignored);
	TestEqual(TEXT("Next expected does not move backward"), Result.NewNextExpectedIndex, NextExpected);

	// Driving the whole course backward from the finish line crosses
	// checkpoints in descending order; none of them ever match NextExpected,
	// so a full reverse lap produces zero accepted crossings.
	const int32 StillExpected = 1; // fresh racer, has not started
	bool bAnyAccepted = false;
	for (int32 Checkpoint = TestCheckpointCount - 1; Checkpoint >= 1; --Checkpoint)
	{
		const FMidanCheckpointCrossResult ReverseResult =
			UMidanLapTimingSubsystem::EvaluateCheckpointCross(StillExpected, Checkpoint, TestCheckpointCount);
		bAnyAccepted |= (ReverseResult.Outcome != EMidanCheckpointCrossOutcome::Ignored);
	}
	TestFalse(TEXT("A full reverse lap never matches the forward-expected sequence"), bAnyAccepted);

	return true;
}

// ---------------------------------------------------------------------------
// Off-track grace accumulation and invalidation.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanOffTrackGraceTest,
	"Midan.Race.LapTiming.OffTrackGraceAccumulatesAndResets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanOffTrackGraceTest::RunTest(const FString& Parameters)
{
	constexpr float GraceLimit = 1.0f;
	constexpr float PollInterval = 0.25f;

	float Timer = 0.f;
	bool bInvalidated = false;

	// On-track: timer stays at zero regardless of starting value.
	Timer = UMidanLapTimingSubsystem::AccumulateOffTrackGrace(0.5f, /*bIsOffTrack=*/false, PollInterval, GraceLimit, bInvalidated);
	TestEqual(TEXT("Being on-track resets the grace timer to zero"), Timer, 0.f);
	TestFalse(TEXT("On-track never invalidates"), bInvalidated);

	// Off-track for three polls (0.75s) stays under the 1.0s limit.
	Timer = 0.f;
	for (int32 i = 0; i < 3; ++i)
	{
		Timer = UMidanLapTimingSubsystem::AccumulateOffTrackGrace(Timer, true, PollInterval, GraceLimit, bInvalidated);
	}
	TestEqual(TEXT("Three polls off-track accumulate 0.75s"), Timer, 0.75f);
	TestFalse(TEXT("Under the grace limit does not invalidate"), bInvalidated);

	// A fourth poll crosses the 1.0s limit.
	Timer = UMidanLapTimingSubsystem::AccumulateOffTrackGrace(Timer, true, PollInterval, GraceLimit, bInvalidated);
	TestTrue(TEXT("Crossing the grace limit invalidates"), bInvalidated);

	// Recovering resets the budget entirely — this is a grace PERIOD, not a
	// cumulative budget across the whole lap.
	Timer = UMidanLapTimingSubsystem::AccumulateOffTrackGrace(Timer, false, PollInterval, GraceLimit, bInvalidated);
	TestEqual(TEXT("Recovering resets the timer even after invalidation"), Timer, 0.f);
	TestFalse(TEXT("Recovering never itself reports invalidation"), bInvalidated);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
