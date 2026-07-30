// Automation Specs for the vehicle Data Asset validation rules.
//
// These run without a world, a map, or a spawned vehicle — the validation
// contract is pure logic over a UObject, which is why it was designed as a
// method on the asset rather than something that inspects a live component.
//
// Method: build a KNOWN-GOOD asset, assert it validates clean, then perturb one
// field at a time and assert the specific rule fires. Perturbing one field at a
// time is the point — a test that breaks three things and checks "there is an
// error" would pass even if the wrong rule fired.

#include "Misc/AutomationTest.h"
#include "VehicleSetupDataAsset.h"
#include "VehicleFeelDataAsset.h"
#include "SurfaceResponseDataAsset.h"
#include "MidanGameplayTags.h"
#include "MidanCurveUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MidanVehicleTestHelpers
{
	void AddCurveKey(FRuntimeFloatCurve& Curve, float Time, float Value)
	{
		if (FRichCurve* Rich = Curve.GetRichCurve())
		{
			Rich->AddKey(Time, Value);
		}
	}

	/**
	 * A setup asset that passes validation with zero errors.
	 *
	 * Modelled on the GT car from docs/VEHICLE_SPEC.md: RWD, ~1550kg, ~500hp,
	 * flat wide torque curve, rear friction below front for oversteer character.
	 * Every perturbation test starts from this and changes exactly one thing.
	 */
	UVehicleSetupDataAsset* MakeValidSetup()
	{
		UVehicleSetupDataAsset* Setup = NewObject<UVehicleSetupDataAsset>();

		Setup->DisplayName = FText::FromString(TEXT("Test GT"));
		Setup->VehicleClass = MidanTags::Vehicle_Class_GT;

		Setup->Mass.MassKg = 1550.f;
		Setup->Mass.CentreOfMassOffset = FVector(0.f, 0.f, -5.f);
		Setup->Mass.InertiaTensorScale = FVector(1.f, 1.f, 1.f);

		// Flat, wide torque curve spanning idle to redline.
		Setup->Powertrain.EngineIdleRPM = 900.f;
		Setup->Powertrain.MaxRPM = 7500.f;
		AddCurveKey(Setup->Powertrain.TorqueCurve, 900.f, 380.f);
		AddCurveKey(Setup->Powertrain.TorqueCurve, 3000.f, 520.f);
		AddCurveKey(Setup->Powertrain.TorqueCurve, 5500.f, 540.f);
		AddCurveKey(Setup->Powertrain.TorqueCurve, 7500.f, 470.f);
		Setup->Powertrain.ForwardGearRatios = { 3.2f, 2.1f, 1.5f, 1.15f, 0.92f, 0.75f };
		Setup->Powertrain.FinalDriveRatio = 3.6f;
		Setup->Powertrain.ReverseGearRatio = 3.0f;
		Setup->Powertrain.AutoShiftUpRatio = 0.92f;
		Setup->Powertrain.AutoShiftDownRatio = 0.55f;

		Setup->Drivetrain.Layout = EMidanDrivetrainLayout::RearWheelDrive;

		Setup->Suspension.Front.WheelLoadRatio = 0.48f;
		Setup->Suspension.Rear.WheelLoadRatio = 0.52f;
		Setup->Suspension.Front.SpringRate = 320.f;
		Setup->Suspension.Rear.SpringRate = 340.f;
		Setup->Suspension.Front.DampingRatio = 0.7f;
		Setup->Suspension.Rear.DampingRatio = 0.72f;

		// Front friction above rear: oversteer character, ratio ~1.09.
		Setup->Tyres.Front.FrictionForceMultiplier = 2.2f;
		Setup->Tyres.Rear.FrictionForceMultiplier = 2.02f;
		AddCurveKey(Setup->Tyres.Front.LateralSlipGraph, 0.f, 0.f);
		AddCurveKey(Setup->Tyres.Front.LateralSlipGraph, 8.f, 1.f);
		AddCurveKey(Setup->Tyres.Front.LateralSlipGraph, 30.f, 0.75f);
		AddCurveKey(Setup->Tyres.Rear.LateralSlipGraph, 0.f, 0.f);
		AddCurveKey(Setup->Tyres.Rear.LateralSlipGraph, 9.f, 1.f);
		AddCurveKey(Setup->Tyres.Rear.LateralSlipGraph, 30.f, 0.8f);
		Setup->Tyres.Front.MaxBrakeTorque = 3400.f;
		Setup->Tyres.Rear.MaxBrakeTorque = 2000.f;
		Setup->Tyres.Rear.MaxHandbrakeTorque = 4000.f;

		// Steering curve: full lock at rest decaying with speed, never rising.
		Setup->Steering.ValidationTopSpeedKmh = 300.f;
		AddCurveKey(Setup->Steering.SteeringCurve, 0.f, 1.f);
		AddCurveKey(Setup->Steering.SteeringCurve, 80.f, 0.6f);
		AddCurveKey(Setup->Steering.SteeringCurve, 180.f, 0.35f);
		AddCurveKey(Setup->Steering.SteeringCurve, 300.f, 0.22f);
		AddCurveKey(Setup->Steering.KeyboardShapingCurve, 0.f, 0.f);
		AddCurveKey(Setup->Steering.KeyboardShapingCurve, 0.15f, 1.f);
		Setup->Steering.SteeringInputRiseRate = 6.f;
		Setup->Steering.SteeringInputFallRate = 8.f;

		// Gearing must not permit more than ValidationTopSpeedKmh.
		Setup->Tyres.Front.WheelRadiusCm = 34.f;
		Setup->Tyres.Rear.WheelRadiusCm = 34.f;

		return Setup;
	}

	/** Count errors only; warnings are expected on a valid-but-incomplete asset
	 *  (no mesh, no feel asset) and must not fail a test. */
	int32 CountErrors(const UVehicleSetupDataAsset* Setup)
	{
		FMidanValidationResult Result;
		Setup->ValidateMidanData(Result);
		return Result.CountErrors();
	}

	/** True when any error's context contains the substring. */
	bool HasErrorContaining(const UMidanDataAsset* Asset, const FString& ContextSubstring)
	{
		FMidanValidationResult Result;
		Asset->ValidateMidanData(Result);

		for (const FMidanValidationIssue& Issue : Result.Issues)
		{
			if (Issue.bIsError && Issue.Context.ToString().Contains(ContextSubstring))
			{
				return true;
			}
		}
		return false;
	}
}

using namespace MidanVehicleTestHelpers;

// ---------------------------------------------------------------------------
// Baseline: the known-good asset must be clean.
//
// This test exists to protect every other test in the file. If the baseline
// drifts into producing errors, every perturbation test still "passes" while
// actually testing nothing.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanVehicleValidBaselineTest,
	"Midan.Vehicle.Validation.ValidBaselineHasNoErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanVehicleValidBaselineTest::RunTest(const FString& Parameters)
{
	UVehicleSetupDataAsset* Setup = MakeValidSetup();

	FMidanValidationResult Result;
	Setup->ValidateMidanData(Result);

	if (Result.HasErrors())
	{
		for (const FMidanValidationIssue& Issue : Result.Issues)
		{
			if (Issue.bIsError)
			{
				AddError(FString::Printf(TEXT("Baseline produced an error at [%s]: %s"),
					*Issue.Context.ToString(), *Issue.Message.ToString()));
			}
		}
	}

	TestEqual(TEXT("A known-good setup must produce zero validation errors"), Result.CountErrors(), 0);
	return true;
}

// ---------------------------------------------------------------------------
// Mass
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanVehicleMassTest,
	"Midan.Vehicle.Validation.RejectsNonPositiveMass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanVehicleMassTest::RunTest(const FString& Parameters)
{
	UVehicleSetupDataAsset* Setup = MakeValidSetup();
	Setup->Mass.MassKg = 0.f;

	TestTrue(TEXT("Zero mass must be an error, because it makes the solver produce NaN"),
		HasErrorContaining(Setup, TEXT("MassKg")));

	Setup->Mass.MassKg = -100.f;
	TestTrue(TEXT("Negative mass must be an error"), HasErrorContaining(Setup, TEXT("MassKg")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanVehicleHighComTest,
	"Midan.Vehicle.Validation.HighCentreOfMassWarnsButDoesNotError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanVehicleHighComTest::RunTest(const FString& Parameters)
{
	// The rally car deliberately wants a high COM. If this ever errors, the
	// validator is fighting the design it exists to protect.
	UVehicleSetupDataAsset* Setup = MakeValidSetup();
	Setup->Mass.CentreOfMassOffset = FVector(0.f, 0.f, 45.f);

	FMidanValidationResult Result;
	Setup->ValidateMidanData(Result);

	TestEqual(TEXT("A high centre of mass must not produce an error"), Result.CountErrors(), 0);
	TestTrue(TEXT("A high centre of mass must produce a warning"), Result.CountWarnings() > 0);
	return true;
}

// ---------------------------------------------------------------------------
// Powertrain
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanVehicleGearOrderTest,
	"Midan.Vehicle.Validation.RejectsNonMonotonicGearRatios",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanVehicleGearOrderTest::RunTest(const FString& Parameters)
{
	UVehicleSetupDataAsset* Setup = MakeValidSetup();

	// Fourth gear taller than third: the automatic gearbox would oscillate.
	Setup->Powertrain.ForwardGearRatios = { 3.2f, 2.1f, 1.5f, 1.7f, 0.92f, 0.75f };

	TestTrue(TEXT("Out-of-order gear ratios must be an error"),
		HasErrorContaining(Setup, TEXT("ForwardGearRatios")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanVehicleTorqueCurveDomainTest,
	"Midan.Vehicle.Validation.RejectsTorqueCurveNotCoveringRevRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanVehicleTorqueCurveDomainTest::RunTest(const FString& Parameters)
{
	UVehicleSetupDataAsset* Setup = MakeValidSetup();

	// Curve stops at 6000 but the engine revs to 7500 — the classic "car dies at
	// the top end" bug, which is silent at runtime because the curve clamps.
	Setup->Powertrain.TorqueCurve.GetRichCurve()->Reset();
	AddCurveKey(Setup->Powertrain.TorqueCurve, 900.f, 380.f);
	AddCurveKey(Setup->Powertrain.TorqueCurve, 6000.f, 520.f);

	TestTrue(TEXT("A torque curve that stops short of MaxRPM must be an error"),
		HasErrorContaining(Setup, TEXT("TorqueCurve")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanVehicleShiftHysteresisTest,
	"Midan.Vehicle.Validation.RejectsInvertedAutoShiftPoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanVehicleShiftHysteresisTest::RunTest(const FString& Parameters)
{
	UVehicleSetupDataAsset* Setup = MakeValidSetup();
	Setup->Powertrain.AutoShiftUpRatio = 0.5f;
	Setup->Powertrain.AutoShiftDownRatio = 0.8f;

	TestTrue(TEXT("Shift-down above shift-up must be an error; without hysteresis the gearbox hunts"),
		HasErrorContaining(Setup, TEXT("AutoShiftDownRatio")));
	return true;
}

// ---------------------------------------------------------------------------
// Steering — the mandatory curve
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanVehicleSteeringCurveMandatoryTest,
	"Midan.Vehicle.Validation.RequiresSteeringCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanVehicleSteeringCurveMandatoryTest::RunTest(const FString& Parameters)
{
	UVehicleSetupDataAsset* Setup = MakeValidSetup();
	Setup->Steering.SteeringCurve.GetRichCurve()->Reset();

	TestTrue(TEXT("A missing steering curve must be an ERROR, not a warning — it is the most common cause of a UE5 car feeling wrong"),
		HasErrorContaining(Setup, TEXT("SteeringCurve")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanVehicleSteeringMonotonicTest,
	"Midan.Vehicle.Validation.RejectsSteeringCurveThatRisesWithSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanVehicleSteeringMonotonicTest::RunTest(const FString& Parameters)
{
	UVehicleSetupDataAsset* Setup = MakeValidSetup();

	// More lock at 300km/h than at 180: never correct.
	Setup->Steering.SteeringCurve.GetRichCurve()->Reset();
	AddCurveKey(Setup->Steering.SteeringCurve, 0.f, 1.f);
	AddCurveKey(Setup->Steering.SteeringCurve, 180.f, 0.35f);
	AddCurveKey(Setup->Steering.SteeringCurve, 300.f, 0.8f);

	TestTrue(TEXT("A steering curve that grants more lock at higher speed must be an error"),
		HasErrorContaining(Setup, TEXT("SteeringCurve")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanVehicleTopSpeedCoverageTest,
	"Midan.Vehicle.Validation.DetectsGearingExceedingSteeringCurveDomain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanVehicleTopSpeedCoverageTest::RunTest(const FString& Parameters)
{
	UVehicleSetupDataAsset* Setup = MakeValidSetup();

	// A very tall top gear pushes achievable speed well past the validated
	// range, so the steer limit would clamp at exactly the worst moment. This is
	// a cross-struct rule: neither the steering nor the powertrain struct can
	// catch it alone.
	Setup->Powertrain.ForwardGearRatios = { 3.2f, 2.1f, 1.5f, 1.15f, 0.92f, 0.30f };

	TestTrue(TEXT("Gearing permitting more speed than the steering curve covers must be an error"),
		HasErrorContaining(Setup, TEXT("ValidationTopSpeedKmh")));
	return true;
}

// ---------------------------------------------------------------------------
// Tyres and balance
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanVehicleBalanceDerivationTest,
	"Midan.Vehicle.Validation.HandlingBalanceDerivesFromFrictionRatio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanVehicleBalanceDerivationTest::RunTest(const FString& Parameters)
{
	UVehicleSetupDataAsset* Setup = MakeValidSetup();

	Setup->Tyres.Front.FrictionForceMultiplier = 2.2f;
	Setup->Tyres.Rear.FrictionForceMultiplier = 2.0f;

	// Derived rather than stored, so it can never disagree with its inputs.
	TestEqual(TEXT("Balance must equal front/rear friction ratio"),
		Setup->GetHandlingBalance(), 1.1f, 0.001f);

	TestTrue(TEXT("Front friction above rear must read as oversteer (>1.0)"),
		Setup->GetHandlingBalance() > 1.f);

	Setup->Tyres.Front.FrictionForceMultiplier = 1.8f;
	TestTrue(TEXT("Front friction below rear must read as understeer (<1.0)"),
		Setup->GetHandlingBalance() < 1.f);

	// Guarded against divide-by-zero rather than returning inf.
	Setup->Tyres.Rear.FrictionForceMultiplier = 0.f;
	TestEqual(TEXT("Zero rear friction must yield 0, not infinity"),
		Setup->GetHandlingBalance(), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanVehicleBrakeBiasTest,
	"Midan.Vehicle.Validation.WarnsOnRearBiasedBraking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanVehicleBrakeBiasTest::RunTest(const FString& Parameters)
{
	UVehicleSetupDataAsset* Setup = MakeValidSetup();
	Setup->Tyres.Front.MaxBrakeTorque = 1500.f;
	Setup->Tyres.Rear.MaxBrakeTorque = 3500.f;

	TestTrue(TEXT("Front brake bias below 0.5 must be detected"),
		Setup->Tyres.GetFrontBrakeBias() < 0.5f);

	FMidanValidationResult Result;
	Setup->ValidateMidanData(Result);
	TestTrue(TEXT("Rear-biased braking must warn"), Result.CountWarnings() > 0);
	return true;
}

// ---------------------------------------------------------------------------
// Suspension cross-struct rule
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanVehicleLoadRatioTest,
	"Midan.Vehicle.Validation.RequiresWheelLoadRatiosToSumToOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanVehicleLoadRatioTest::RunTest(const FString& Parameters)
{
	UVehicleSetupDataAsset* Setup = MakeValidSetup();
	Setup->Suspension.Front.WheelLoadRatio = 0.3f;
	Setup->Suspension.Rear.WheelLoadRatio = 0.3f;

	TestTrue(TEXT("Load ratios that do not sum to 1.0 must be an error — every tyre value is calibrated against axle load"),
		HasErrorContaining(Setup, TEXT("WheelLoadRatio")));
	return true;
}

// ---------------------------------------------------------------------------
// Vehicle class tag
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanVehicleClassTagTest,
	"Midan.Vehicle.Validation.RejectsInvalidVehicleClassTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanVehicleClassTagTest::RunTest(const FString& Parameters)
{
	UVehicleSetupDataAsset* Setup = MakeValidSetup();

	Setup->VehicleClass = FGameplayTag();
	TestTrue(TEXT("An unset vehicle class must be an error"),
		HasErrorContaining(Setup, TEXT("VehicleClass")));

	// The parent tag is a grouping node, not an assignable class.
	Setup->VehicleClass = MidanTags::Vehicle_Class;
	TestTrue(TEXT("Assigning the parent Vehicle.Class tag directly must be an error"),
		HasErrorContaining(Setup, TEXT("VehicleClass")));

	Setup->VehicleClass = MidanTags::Vehicle_Class_Rally;
	TestFalse(TEXT("A valid leaf class tag must not error"),
		HasErrorContaining(Setup, TEXT("VehicleClass")));
	return true;
}

// ---------------------------------------------------------------------------
// Surface response
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanSurfaceCoverageTest,
	"Midan.Vehicle.Validation.RequiresEverySurfaceTagToHaveARow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanSurfaceCoverageTest::RunTest(const FString& Parameters)
{
	USurfaceResponseDataAsset* Asset = NewObject<USurfaceResponseDataAsset>();

	FMidanValidationResult EmptyResult;
	Asset->ValidateMidanData(EmptyResult);
	TestTrue(TEXT("An empty surface table must be an error"), EmptyResult.HasErrors());

	// Only tarmac mapped: the other five surfaces would silently fall back.
	FSurfaceResponseRow Tarmac;
	Tarmac.SurfaceTag = MidanTags::Surface_Tarmac;
	Tarmac.PhysicalSurface = SurfaceType1;
	Tarmac.FrictionMultiplier = 1.f;
	Tarmac.bCountsAsOnTrack = true;
	Asset->Surfaces.Add(Tarmac);

	TestTrue(TEXT("A partially populated surface table must still error on the missing rows"),
		HasErrorContaining(Asset, TEXT("Surfaces")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanSurfaceLookupFallbackTest,
	"Midan.Vehicle.Validation.SurfaceLookupFallsBackRatherThanReturningNull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanSurfaceLookupFallbackTest::RunTest(const FString& Parameters)
{
	USurfaceResponseDataAsset* Asset = NewObject<USurfaceResponseDataAsset>();

	TestNull(TEXT("An empty table must return null"), Asset->FindRow(SurfaceType1));

	FSurfaceResponseRow Tarmac;
	Tarmac.SurfaceTag = MidanTags::Surface_Tarmac;
	Tarmac.PhysicalSurface = SurfaceType1;
	Asset->Surfaces.Add(Tarmac);

	TestNotNull(TEXT("A mapped surface must resolve"), Asset->FindRow(SurfaceType1));

	// An unmapped surface falls back to the first row rather than returning
	// null, so a wheel over unmapped geometry behaves like the default surface
	// instead of losing all grip. Validation reports the gap separately.
	const FSurfaceResponseRow* Fallback = Asset->FindRow(SurfaceType5);
	TestNotNull(TEXT("An unmapped surface must fall back rather than return null"), Fallback);
	if (Fallback)
	{
		TestEqual(TEXT("The fallback must be the first row"), Fallback->SurfaceTag, MidanTags::Surface_Tarmac);
	}
	return true;
}

// ---------------------------------------------------------------------------
// Curve utilities — the primitives every rule above depends on
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanCurveUtilsTest,
	"Midan.Core.CurveUtils.DomainAndMonotonicityChecks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanCurveUtilsTest::RunTest(const FString& Parameters)
{
	FRuntimeFloatCurve Empty;
	TestFalse(TEXT("An empty curve reports no keys"), MidanCurve::HasKeys(Empty));
	TestEqual(TEXT("EvalSafe returns the fallback for an empty curve"),
		MidanCurve::EvalSafe(Empty, 5.f, -1.f), -1.f);

	FRuntimeFloatCurve Curve;
	AddCurveKey(Curve, 0.f, 10.f);
	AddCurveKey(Curve, 100.f, 20.f);

	TestTrue(TEXT("A populated curve reports keys"), MidanCurve::HasKeys(Curve));

	float Min = 0.f;
	float Max = 0.f;
	TestTrue(TEXT("GetDomain succeeds"), MidanCurve::GetDomain(Curve, Min, Max));
	TestEqual(TEXT("Domain min"), Min, 0.f);
	TestEqual(TEXT("Domain max"), Max, 100.f);

	// Domain coverage
	{
		FMidanValidationResult Result;
		MidanCurve::ValidateDomainCovers(Curve, 0.f, 100.f, TEXT("Test"), Result);
		TestFalse(TEXT("A curve exactly covering the required range passes"), Result.HasErrors());
	}
	{
		FMidanValidationResult Result;
		MidanCurve::ValidateDomainCovers(Curve, 0.f, 200.f, TEXT("Test"), Result);
		TestTrue(TEXT("A curve short of the required range fails"), Result.HasErrors());
	}

	// Monotonicity
	{
		FMidanValidationResult Result;
		MidanCurve::ValidateMonotonicNonIncreasing(Curve, TEXT("Test"), Result);
		TestTrue(TEXT("A rising curve fails the non-increasing check"), Result.HasErrors());
	}
	{
		FRuntimeFloatCurve Falling;
		AddCurveKey(Falling, 0.f, 1.f);
		AddCurveKey(Falling, 100.f, 0.2f);

		FMidanValidationResult Result;
		MidanCurve::ValidateMonotonicNonIncreasing(Falling, TEXT("Test"), Result);
		TestFalse(TEXT("A falling curve passes the non-increasing check"), Result.HasErrors());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
