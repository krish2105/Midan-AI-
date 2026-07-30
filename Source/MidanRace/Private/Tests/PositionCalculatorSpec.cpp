// Automation Specs for MidanPositionCalculator — pure functions, no UWorld,
// no map, no spawned vehicle. See docs/ARCHITECTURE.md §5.

#include "Misc/AutomationTest.h"
#include "MidanPositionCalculator.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanTotalProgressTest,
	"Midan.Race.Position.TotalProgressIsMonotonicAcrossLaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanTotalProgressTest::RunTest(const FString& Parameters)
{
	constexpr float TrackLength = 3000.f;

	const float LapZeroHalfway = MidanRacePosition::ComputeTotalProgress(0, 1500.f, TrackLength);
	const float LapOneStart = MidanRacePosition::ComputeTotalProgress(1, 0.f, TrackLength);
	const float LapOneHalfway = MidanRacePosition::ComputeTotalProgress(1, 1500.f, TrackLength);

	TestEqual(TEXT("Lap 0, halfway"), LapZeroHalfway, 1500.f);
	TestEqual(TEXT("Lap 1, at the line"), LapOneStart, 3000.f);
	TestEqual(TEXT("Lap 1, halfway"), LapOneHalfway, 4500.f);

	// The whole point of total progress: a racer on lap 1 who has barely
	// crossed the line outranks a racer still finishing lap 0, however close
	// to the line the lap-0 racer is.
	TestTrue(TEXT("Crossing into a new lap always outranks any position in the previous lap"), LapOneStart > LapZeroHalfway);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanRacePositionsOrderingTest,
	"Midan.Race.Position.RacePositionsOrderByProgressDescending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanRacePositionsOrderingTest::RunTest(const FString& Parameters)
{
	// Index 0 is furthest ahead, index 3 is last.
	const TArray<float> Progress = { 5000.f, 3000.f, 4200.f, 100.f };
	const TArray<int32> Positions = MidanRacePosition::ComputeRacePositions(Progress);

	TestEqual(TEXT("One position per racer"), Positions.Num(), Progress.Num());
	TestEqual(TEXT("Furthest progress is P1"), Positions[0], 1);
	TestEqual(TEXT("Second furthest is P2"), Positions[2], 2);
	TestEqual(TEXT("Third furthest is P3"), Positions[1], 3);
	TestEqual(TEXT("Least progress is P4 (last)"), Positions[3], 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanRacePositionsTieBreakTest,
	"Midan.Race.Position.TiesBreakByInputOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanRacePositionsTieBreakTest::RunTest(const FString& Parameters)
{
	const TArray<float> Progress = { 1000.f, 1000.f, 500.f };
	const TArray<int32> Positions = MidanRacePosition::ComputeRacePositions(Progress);

	TestEqual(TEXT("Earlier index wins an exact tie"), Positions[0], 1);
	TestEqual(TEXT("Later index at the same progress is second"), Positions[1], 2);
	TestEqual(TEXT("Clearly behind racer is third regardless of tie above it"), Positions[2], 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanRacePositionsEdgeCasesTest,
	"Midan.Race.Position.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanRacePositionsEdgeCasesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Empty input returns empty output"), MidanRacePosition::ComputeRacePositions({}).Num(), 0);

	const TArray<int32> SingleRacer = MidanRacePosition::ComputeRacePositions({ 42.f });
	TestEqual(TEXT("A single racer is always P1"), SingleRacer.Num() == 1 && SingleRacer[0] == 1, true);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
