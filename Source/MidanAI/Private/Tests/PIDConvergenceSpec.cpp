// Automation Specs for FMidanPIDController: convergence against a simple
// simulated plant, and anti-windup clamping. Pure struct, no UWorld — see
// MidanPIDController.h. This is the Phase 6 gate's other deliverable: "the
// PID convergence test results."

#include "Misc/AutomationTest.h"
#include "MidanPIDController.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * A first-order plant: Measured moves toward (Measured + Control) each
	 * step, scaled by a response rate — a reasonable stand-in for "throttle
	 * input produces a proportional change in speed" without needing the
	 * actual vehicle physics this struct is deliberately decoupled from.
	 */
	float StepPlant(float Measured, float Control, float ResponseRate, float DeltaSeconds)
	{
		return Measured + Control * ResponseRate * DeltaSeconds;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanPIDConvergesToSetpointTest,
	"Midan.AI.PID.ConvergesToSetpointWithinTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanPIDConvergesToSetpointTest::RunTest(const FString& Parameters)
{
	FMidanPIDController PID;
	PID.ProportionalGain = 0.6f;
	PID.IntegralGain = 0.4f;
	PID.DerivativeGain = 0.05f;
	PID.IntegralClamp = 10.f;

	constexpr float SetPoint = 100.f;
	constexpr float DeltaSeconds = 1.f / 30.f;
	constexpr float ResponseRate = 1.f;
	constexpr int32 StepCount = 300; // 10 simulated seconds at 30Hz

	float Measured = 0.f;

	for (int32 i = 0; i < StepCount; ++i)
	{
		const float Error = SetPoint - Measured;
		const float Control = PID.Update(Error, DeltaSeconds);
		Measured = StepPlant(Measured, Control, ResponseRate, DeltaSeconds);
	}

	AddInfo(FString::Printf(TEXT("PID convergence: setpoint=%.1f, measured after %d steps=%.3f, error=%.4f"),
		SetPoint, StepCount, Measured, SetPoint - Measured));

	TestTrue(TEXT("Converges to within 1% of setpoint"), FMath::Abs(SetPoint - Measured) < SetPoint * 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanPIDConvergesFromOvershootTest,
	"Midan.AI.PID.ConvergesFromAnOvershotStartingPoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanPIDConvergesFromOvershootTest::RunTest(const FString& Parameters)
{
	// Starts ABOVE the setpoint — exercises the controller pulling down, not
	// just up, which a throttle-only test would miss (this struct is generic;
	// AMidanOpponentController is what decides whether "pull down" means
	// braking or lifting off, not this controller).
	FMidanPIDController PID;
	PID.ProportionalGain = 0.6f;
	PID.IntegralGain = 0.4f;
	PID.DerivativeGain = 0.05f;
	PID.IntegralClamp = 10.f;

	constexpr float SetPoint = 50.f;
	constexpr float DeltaSeconds = 1.f / 30.f;
	constexpr float ResponseRate = 1.f;
	constexpr int32 StepCount = 300;

	float Measured = 150.f;

	for (int32 i = 0; i < StepCount; ++i)
	{
		const float Error = SetPoint - Measured;
		const float Control = PID.Update(Error, DeltaSeconds);
		Measured = StepPlant(Measured, Control, ResponseRate, DeltaSeconds);
	}

	AddInfo(FString::Printf(TEXT("PID convergence from overshoot: setpoint=%.1f, measured after %d steps=%.3f"),
		SetPoint, StepCount, Measured));

	TestTrue(TEXT("Converges to within 1%% of setpoint from an overshot start"), FMath::Abs(SetPoint - Measured) < SetPoint * 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanPIDAntiWindupTest,
	"Midan.AI.PID.IntegralNeverExceedsClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanPIDAntiWindupTest::RunTest(const FString& Parameters)
{
	FMidanPIDController PID;
	PID.ProportionalGain = 0.1f;
	PID.IntegralGain = 1.f;
	PID.DerivativeGain = 0.f;
	PID.IntegralClamp = 2.f;

	// A sustained, unachievable error (the "measured" value never moves at
	// all — e.g. a car stuck against a wall) is exactly the scenario
	// anti-windup exists for: without the clamp, the integral term grows
	// without bound and the controller overshoots wildly once the error
	// finally clears.
	constexpr float DeltaSeconds = 1.f / 30.f;
	constexpr int32 StepCount = 900; // 30 simulated seconds — long enough to wind up badly if unclamped

	float LastControl = 0.f;
	for (int32 i = 0; i < StepCount; ++i)
	{
		LastControl = PID.Update(1000.f, DeltaSeconds); // Measured never moves; error stays huge
	}

	// With IntegralGain=1 and IntegralClamp=2, the integral term alone can
	// contribute at most 2.0. Proportional adds 0.1*1000=100 on top, which is
	// expected and correct (proportional response to a genuinely large error
	// is not windup) — what must NOT happen is the integral term itself
	// growing past its clamp after 900 sustained-error steps.
	const float ProportionalContribution = PID.ProportionalGain * 1000.f;
	const float ImpliedIntegralContribution = LastControl - ProportionalContribution;

	AddInfo(FString::Printf(TEXT("Anti-windup: implied integral contribution after %d steps of sustained error = %.4f (clamp = %.1f)"),
		StepCount, ImpliedIntegralContribution, PID.IntegralClamp));

	TestTrue(TEXT("Integral contribution never exceeds IntegralGain * IntegralClamp"),
		ImpliedIntegralContribution <= (PID.IntegralGain * PID.IntegralClamp) + KINDA_SMALL_NUMBER);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanPIDResetClearsStateTest,
	"Midan.AI.PID.ResetClearsIntegralAndDerivativeHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanPIDResetClearsStateTest::RunTest(const FString& Parameters)
{
	FMidanPIDController PID;
	PID.ProportionalGain = 0.f;
	PID.IntegralGain = 1.f;
	PID.DerivativeGain = 0.f;
	PID.IntegralClamp = 100.f;

	// Wind up a non-trivial integral term.
	for (int32 i = 0; i < 30; ++i)
	{
		PID.Update(10.f, 1.f / 30.f);
	}

	PID.Reset();

	// Immediately after Reset, a zero error must produce a zero output —
	// proves the integral accumulator was actually cleared, not just that
	// the next call happens to look small.
	const float ControlAfterReset = PID.Update(0.f, 1.f / 30.f);

	TestEqual(TEXT("Zero error immediately after Reset produces zero output"), ControlAfterReset, 0.f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
