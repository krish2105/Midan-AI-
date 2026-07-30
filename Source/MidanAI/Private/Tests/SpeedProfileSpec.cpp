// Automation Specs for MidanSpeedProfileGenerator, against a synthetic corner
// sequence. Pure math, no UWorld, no racing line, no vehicle — see the header
// comment in MidanSpeedProfileGenerator.h. This is the Phase 6 gate
// deliverable: "print the generated speed profile for a synthetic corner
// sequence."

#include "Misc/AutomationTest.h"
#include "MidanSpeedProfileGenerator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * A closed 4000cm (40m) loop: samples 0-4 and 10-19 are a straight
	 * (curvature 0), samples 5-9 are a 500cm-radius corner (curvature
	 * 1/500 = 0.002 per cm, tight enough to force real braking). Spaced
	 * 200cm apart, 20 samples total.
	 *
	 * VMax and MaxBrakingDeceleration are deliberately modest (60km/h,
	 * 2000cm/s^2) rather than realistic vehicle numbers — the point of this
	 * synthetic sequence is a short enough loop that the profile visibly
	 * recovers to VMax on the straight and visibly brakes on approach to the
	 * corner, both within the loop's own length, so the test is legible
	 * without needing a kilometre-scale synthetic track.
	 */
	TArray<MidanSpeedProfile::FCurvatureSample> MakeSyntheticCornerSequence()
	{
		TArray<MidanSpeedProfile::FCurvatureSample> Samples;
		constexpr int32 Count = 20;
		constexpr float SpacingCm = 200.f;
		constexpr float CornerRadiusCm = 500.f;

		for (int32 i = 0; i < Count; ++i)
		{
			MidanSpeedProfile::FCurvatureSample Sample;
			Sample.ArcLengthCm = i * SpacingCm;
			Sample.Curvature = (i >= 5 && i <= 9) ? (1.f / CornerRadiusCm) : 0.f;
			Samples.Add(Sample);
		}
		return Samples;
	}

	constexpr float TestMuEffective = 1.f;
	constexpr float TestVMaxKmh = 60.f;
	constexpr float TestMaxBrakingDecelCmS2 = 2000.f;
	constexpr float TestTrackLengthCm = 4000.f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanCorneringLimitTest,
	"Midan.AI.SpeedProfile.CorneringLimitIsLowInTheCornerHighOnStraights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanCorneringLimitTest::RunTest(const FString& Parameters)
{
	const TArray<MidanSpeedProfile::FCurvatureSample> Samples = MakeSyntheticCornerSequence();

	TArray<float> CorneringLimitKmh;
	MidanSpeedProfile::ComputeCorneringLimit(Samples, TestMuEffective, TestVMaxKmh, CorneringLimitKmh);

	TestEqual(TEXT("One limit per sample"), CorneringLimitKmh.Num(), Samples.Num());

	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		const bool bInCorner = (i >= 5 && i <= 9);
		if (bInCorner)
		{
			TestTrue(FString::Printf(TEXT("Sample %d is in the corner and well under VMax"), i), CorneringLimitKmh[i] < TestVMaxKmh * 0.5f);
		}
		else
		{
			TestEqual(FString::Printf(TEXT("Sample %d is on the straight, uncapped by grip"), i), CorneringLimitKmh[i], TestVMaxKmh);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanBackwardBrakingPassTest,
	"Midan.AI.SpeedProfile.BackwardPassBrakesBeforeTheCornerNotAfter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanBackwardBrakingPassTest::RunTest(const FString& Parameters)
{
	const TArray<MidanSpeedProfile::FCurvatureSample> Samples = MakeSyntheticCornerSequence();

	TArray<float> SpeedKmh;
	TArray<bool> BrakingZone;
	MidanSpeedProfile::GenerateSpeedProfile(
		Samples, TestMuEffective, TestVMaxKmh, TestMaxBrakingDecelCmS2, /*bClosedLoop=*/true, TestTrackLengthCm, SpeedKmh, BrakingZone);

	AddInfo(TEXT("Generated speed profile (synthetic 4000cm loop, corner at samples 5-9):"));
	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		AddInfo(FString::Printf(TEXT("  [%2d] arc=%5.0fcm curvature=%.5f speed=%6.1fkm/h braking=%s"),
			i, Samples[i].ArcLengthCm, Samples[i].Curvature, SpeedKmh[i], BrakingZone[i] ? TEXT("true") : TEXT("false")));
	}

	// The sample immediately entering the corner (index 5) must be at (or
	// very near) the corner's own cornering limit — the whole point of the
	// backward pass is that the car has ALREADY slowed by the time it
	// arrives, not that the corner itself gets slower.
	TestTrue(TEXT("Corner entry speed is close to the corner's cornering limit"), SpeedKmh[5] < 30.f);

	// The approach samples (3, 4 — on the straight, just before the corner)
	// must be reduced below VMax: this is the actual "backward pass from the
	// apex" behaviour, braking on the straight in anticipation of a corner
	// that hasn't arrived yet. Speed rises moving AWAY from the corner (more
	// distance available to shed speed under braking), so sample 3 — one
	// step further from the corner than sample 4 — permits a higher entry
	// speed, not a lower one.
	TestTrue(TEXT("Sample 4 (last straight sample before the corner) is braking, not at VMax"), SpeedKmh[4] < TestVMaxKmh);
	TestTrue(TEXT("Sample 4 is flagged as a braking zone"), BrakingZone[4]);
	TestTrue(TEXT("Sample 3 permits a higher speed than sample 4 (further from the corner)"), SpeedKmh[3] >= SpeedKmh[4] - KINDA_SMALL_NUMBER);

	// Far from the corner (the midpoint of the long straight, sample 15 on a
	// 20-sample closed loop with a 5-sample corner) there is enough distance
	// to brake later — this sample should be back at VMax, not braking the
	// entire lap.
	TestEqual(TEXT("Sample 15 (far from the corner on the return straight) is at VMax"), SpeedKmh[15], TestVMaxKmh);
	TestFalse(TEXT("Sample 15 is not flagged as a braking zone"), BrakingZone[15]);

	// Deep in the corner (sample 7, the middle of the tight section) is not
	// itself flagged as "braking" beyond its own cornering limit — it IS the
	// floor, not a point braking toward something slower still.
	TestFalse(TEXT("Sample 7 (corner interior) is at its own cornering limit, not additionally braking"), BrakingZone[7]);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanSpeedProfileOpenSequenceTest,
	"Midan.AI.SpeedProfile.OpenSequenceDoesNotWrapBraking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanSpeedProfileOpenSequenceTest::RunTest(const FString& Parameters)
{
	// Same geometry, but bClosedLoop=false: the last sample has nothing to
	// brake toward, so it must simply be its own cornering limit (VMax, since
	// sample 19 is on the straight) regardless of what comes "after" it on a
	// closed interpretation.
	const TArray<MidanSpeedProfile::FCurvatureSample> Samples = MakeSyntheticCornerSequence();

	TArray<float> SpeedKmh;
	TArray<bool> BrakingZone;
	MidanSpeedProfile::GenerateSpeedProfile(
		Samples, TestMuEffective, TestVMaxKmh, TestMaxBrakingDecelCmS2, /*bClosedLoop=*/false, TestTrackLengthCm, SpeedKmh, BrakingZone);

	TestEqual(TEXT("Last sample on an open sequence has nothing to brake toward"), SpeedKmh.Last(), TestVMaxKmh);
	TestFalse(TEXT("Last sample is not a braking zone on an open sequence"), BrakingZone.Last());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
