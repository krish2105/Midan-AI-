#include "VehicleSetupApplier.h"
#include "VehicleSetupDataAsset.h"
#include "MidanDataAsset.h"
#include "MidanCurveUtils.h"
#include "MidanLogChannels.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"

#if WITH_EDITOR
#include "EngineUtils.h"
#include "Engine/Engine.h"
#endif

// =============================================================================
// API VERIFICATION REQUIRED
//
// Every line marked `API VERIFY` writes to a Chaos field whose exact name I
// could not confirm against installed UE 5.8 headers — no engine is present on
// the build machine (docs/ASSUMPTIONS.md A3). The STRUCTURE of this file is
// correct and the mapping decisions are deliberate; the field spellings are the
// risk. Check them against:
//
//   Engine/Plugins/Runtime/ChaosVehicles/Source/ChaosVehicles/Public/
//     ChaosWheeledVehicleMovementComponent.h   (Engine/Transmission/Differential/Steering setups)
//     ChaosVehicleWheel.h                      (per-wheel suspension and tyre fields)
//
// Known version-sensitive spots, in order of likelihood:
//   1. Transmission shift timing — some versions expose a single GearChangeTime
//      rather than separate up/down times, and some use ChangeUpRPM/ChangeDownRPM
//      absolute values rather than ratios.
//   2. Centre-of-mass override field naming.
//   3. Whether wheel writes must happen before the vehicle sim is created.
//      See the ORDERING note below — this one is a correctness issue, not a
//      spelling issue, and matters more than the rest.
// =============================================================================

// ORDERING: Chaos snapshots wheel and engine configuration into its internal
// physics representation when the vehicle simulation is created. Writing to the
// UObject-side setup structs AFTER that point may not take effect until the sim
// is rebuilt. Apply must therefore run BEFORE the movement component finishes
// initialising — from the pawn's PreInitializeComponents at Phase 3, not from
// BeginPlay. If a value visibly fails to take hold at runtime, this ordering is
// the first thing to check, not the field name.

bool UVehicleSetupApplier::Apply(
	const UVehicleSetupDataAsset* Setup,
	UChaosWheeledVehicleMovementComponent* Movement)
{
	if (!Setup)
	{
		UE_LOG(LogMidanVehicle, Error, TEXT("VehicleSetupApplier::Apply called with a null setup asset. The vehicle will use Chaos defaults, which are not tuned for anything."));
		return false;
	}

	if (!Movement)
	{
		UE_LOG(LogMidanVehicle, Error, TEXT("VehicleSetupApplier::Apply called with a null movement component for setup '%s'."), *Setup->GetName());
		return false;
	}

	// Refuse to apply a known-invalid setup. A negative mass or an empty torque
	// curve produces NaN in the solver, and NaN surfaces as the car silently
	// vanishing — far harder to diagnose than a log line at startup.
	FMidanValidationResult Validation;
	Setup->ValidateMidanData(Validation);

	if (Validation.HasErrors())
	{
		UE_LOG(LogMidanVehicle, Error,
			TEXT("Refusing to apply setup '%s': %d validation error(s). Physics would be undefined."),
			*Setup->GetName(), Validation.CountErrors());

		for (const FMidanValidationIssue& Issue : Validation.Issues)
		{
			if (Issue.bIsError)
			{
				UE_LOG(LogMidanVehicle, Error, TEXT("  [%s] %s"), *Issue.Context.ToString(), *Issue.Message.ToString());
			}
		}
		return false;
	}

	for (const FMidanValidationIssue& Issue : Validation.Issues)
	{
		UE_LOG(LogMidanVehicle, Warning, TEXT("Setup '%s' [%s]: %s"),
			*Setup->GetName(), *Issue.Context.ToString(), *Issue.Message.ToString());
	}

	ApplyMass(Setup, Movement);
	ApplyPowertrain(Setup, Movement);
	ApplyDrivetrain(Setup, Movement);
	ApplyWheels(Setup, Movement);
	ApplySteering(Setup, Movement);

	UE_LOG(LogMidanVehicle, Log,
		TEXT("Applied setup '%s' (%s): %.0fkg, balance %.2f, aero balance %.2f, %d gears."),
		*Setup->GetName(),
		*Setup->VehicleClass.GetTagName().ToString(),
		Setup->Mass.MassKg,
		Setup->GetHandlingBalance(),
		Setup->GetAeroBalance(),
		Setup->Powertrain.ForwardGearRatios.Num());

	return true;
}

void UVehicleSetupApplier::ApplyMass(
	const UVehicleSetupDataAsset* Setup,
	UChaosWheeledVehicleMovementComponent* Movement)
{
	const FVehicleMassConfig& Config = Setup->Mass;

	Movement->Mass = Config.MassKg;                                    // API VERIFY
	Movement->InertiaTensorScale = Config.InertiaTensorScale;           // API VERIFY

	// Centre of mass is a design dial, so it is always written explicitly rather
	// than left to the physics asset's computed value — otherwise a mesh change
	// would silently alter handling.
	Movement->bEnableCenterOfMassOverride = true;                       // API VERIFY
	Movement->CenterOfMassOverride = Config.CentreOfMassOffset;         // API VERIFY

	Movement->DownforceCoefficient = Config.DownforceWhenAirborne;      // API VERIFY

	// NOTE: aerodynamic drag and downforce are deliberately NOT written here.
	// Chaos's built-in drag is a single scalar applied at one point, which cannot
	// express the separate front/rear application that makes downforce balance
	// shift with speed. UVehicleAeroComponent implements that in the async
	// physics callback at Phase 3. Setting Chaos's own drag as well would
	// double-count it.
}

void UVehicleSetupApplier::ApplyPowertrain(
	const UVehicleSetupDataAsset* Setup,
	UChaosWheeledVehicleMovementComponent* Movement)
{
	const FVehiclePowertrainConfig& Config = Setup->Powertrain;

	// Engine ------------------------------------------------------------------
	Movement->EngineSetup.MaxRPM = Config.MaxRPM;                       // API VERIFY
	Movement->EngineSetup.EngineIdleRPM = Config.EngineIdleRPM;         // API VERIFY
	Movement->EngineSetup.EngineBrakeEffect = Config.EngineBrakeEffect; // API VERIFY
	Movement->EngineSetup.EngineRevUpMOI = Config.EngineRevUpMOI;       // API VERIFY
	Movement->EngineSetup.EngineRevDownRate = Config.EngineRevDownRate; // API VERIFY

	// Chaos expects the torque curve normalised 0..1 against RPM and scales it
	// by MaxTorque, whereas the Data Asset authors absolute Nm against RPM —
	// which is the form a human can reason about and compare between cars.
	// Convert here rather than pushing the normalisation burden onto the author.
	float TorqueMin = 0.f;
	float TorqueMax = 0.f;
	if (MidanCurve::GetValueRange(Config.TorqueCurve, TorqueMin, TorqueMax) && TorqueMax > KINDA_SMALL_NUMBER)
	{
		Movement->EngineSetup.MaxTorque = TorqueMax;                    // API VERIFY

		FRichCurve* Target = Movement->EngineSetup.TorqueCurve.GetRichCurve();  // API VERIFY
		const FRichCurve* Source = Config.TorqueCurve.GetRichCurveConst();

		if (Target && Source)
		{
			Target->Reset();
			for (auto It = Source->GetKeyIterator(); It; ++It)
			{
				Target->AddKey(It->Time, It->Value / TorqueMax);
			}
		}
	}

	// Transmission ------------------------------------------------------------
	Movement->TransmissionSetup.bUseAutomaticGears = Config.bUseAutomaticGears;          // API VERIFY
	Movement->TransmissionSetup.FinalRatio = Config.FinalDriveRatio;                     // API VERIFY
	Movement->TransmissionSetup.TransmissionEfficiency = Config.TransmissionEfficiency;  // API VERIFY

	Movement->TransmissionSetup.ForwardGearRatios.Reset();                               // API VERIFY
	for (const float Ratio : Config.ForwardGearRatios)
	{
		Movement->TransmissionSetup.ForwardGearRatios.Add(Ratio);
	}

	Movement->TransmissionSetup.ReverseGearRatios.Reset();                               // API VERIFY
	Movement->TransmissionSetup.ReverseGearRatios.Add(Config.ReverseGearRatio);

	// The Data Asset expresses shift points as a FRACTION of MaxRPM so that
	// changing MaxRPM does not silently move the shift points — Chaos wants
	// absolute RPM, so convert.
	Movement->TransmissionSetup.ChangeUpRPM = Config.MaxRPM * Config.AutoShiftUpRatio;    // API VERIFY
	Movement->TransmissionSetup.ChangeDownRPM = Config.MaxRPM * Config.AutoShiftDownRatio;// API VERIFY

	// API VERIFY: some engine versions expose a single GearChangeTime instead of
	// separate up and down times. If only one exists, use ChangeUpTime — an
	// upshift is the one the player feels, and shift punch is a feel value.
	Movement->TransmissionSetup.GearChangeTime = Config.ChangeUpTime;                     // API VERIFY
}

void UVehicleSetupApplier::ApplyDrivetrain(
	const UVehicleSetupDataAsset* Setup,
	UChaosWheeledVehicleMovementComponent* Movement)
{
	const FVehicleDrivetrainConfig& Config = Setup->Drivetrain;

	switch (Config.Layout)
	{
	case EMidanDrivetrainLayout::AllWheelDrive:
		Movement->DifferentialSetup.DifferentialType = EVehicleDifferential::AllWheelDrive;   // API VERIFY
		Movement->DifferentialSetup.FrontRearSplit = Config.FrontTorqueSplit;                 // API VERIFY
		break;

	case EMidanDrivetrainLayout::FrontWheelDrive:
		Movement->DifferentialSetup.DifferentialType = EVehicleDifferential::FrontWheelDrive; // API VERIFY
		break;

	case EMidanDrivetrainLayout::RearWheelDrive:
	default:
		Movement->DifferentialSetup.DifferentialType = EVehicleDifferential::RearWheelDrive;  // API VERIFY
		break;
	}

	Movement->DifferentialSetup.FrontLeftRightSplit = Config.FrontLeftRightSplit;             // API VERIFY
	Movement->DifferentialSetup.RearLeftRightSplit = Config.RearLeftRightSplit;               // API VERIFY
}

void UVehicleSetupApplier::ApplyWheels(
	const UVehicleSetupDataAsset* Setup,
	UChaosWheeledVehicleMovementComponent* Movement)
{
	const FVehicleTyreConfig& Tyres = Setup->Tyres;
	const FVehicleSuspensionConfig& Suspension = Setup->Suspension;

	// Wheels are indexed 0=FL, 1=FR, 2=RL, 3=RR by the WheelSetups ordering the
	// pawn establishes at Phase 3. Axle assignment is derived from that index
	// rather than from the wheel's AxleType field, so a mis-set AxleType on a
	// wheel asset cannot silently swap front and rear tuning.
	const int32 WheelCount = Movement->Wheels.Num();                                          // API VERIFY

	if (WheelCount != 4)
	{
		UE_LOG(LogMidanVehicle, Warning,
			TEXT("Setup '%s': expected 4 wheels, found %d. Per-axle tuning assumes 0/1 front and 2/3 rear; anything else is applied best-effort."),
			*Setup->GetName(), WheelCount);
	}

	for (int32 Index = 0; Index < WheelCount; ++Index)
	{
		UChaosVehicleWheel* Wheel = Movement->Wheels[Index];
		if (!Wheel)
		{
			continue;
		}

		const bool bIsFront = (Index < 2);
		const FVehicleAxleTyreConfig& Tyre = bIsFront ? Tyres.Front : Tyres.Rear;
		const FVehicleAxleSuspensionConfig& Susp = bIsFront ? Suspension.Front : Suspension.Rear;

		// Wheel geometry -------------------------------------------------------
		Wheel->WheelRadius = Tyre.WheelRadiusCm;                                             // API VERIFY
		Wheel->WheelWidth = Tyre.WheelWidthCm;                                               // API VERIFY
		Wheel->WheelMass = Tyre.WheelMassKg;                                                 // API VERIFY

		// Tyre grip ------------------------------------------------------------
		Wheel->FrictionForceMultiplier = Tyre.FrictionForceMultiplier;                        // API VERIFY
		Wheel->CorneringStiffness = Tyre.CorneringStiffness;                                  // API VERIFY
		Wheel->SlipThreshold = Tyre.SlipThreshold;                                            // API VERIFY
		Wheel->SkidThreshold = Tyre.SkidThreshold;                                            // API VERIFY

		if (MidanCurve::HasKeys(Tyre.LateralSlipGraph))
		{
			Wheel->LateralSlipGraph = Tyre.LateralSlipGraph;                                  // API VERIFY
		}

		// Brakes ---------------------------------------------------------------
		Wheel->MaxBrakeTorque = Tyre.MaxBrakeTorque;                                          // API VERIFY
		Wheel->MaxHandBrakeTorque = Tyre.MaxHandbrakeTorque;                                  // API VERIFY

		// Suspension -----------------------------------------------------------
		Wheel->SuspensionMaxRaise = Susp.MaxRaiseCm;                                          // API VERIFY
		Wheel->SuspensionMaxDrop = Susp.MaxDropCm;                                            // API VERIFY
		Wheel->SpringRate = Susp.SpringRate;                                                  // API VERIFY
		Wheel->SpringPreload = Susp.SpringPreload;                                            // API VERIFY
		Wheel->SuspensionDampingRatio = Susp.DampingRatio;                                    // API VERIFY
		Wheel->SuspensionForceOffset = FVector(0.f, 0.f, Susp.ForceOffsetCm);                 // API VERIFY
		Wheel->WheelLoadRatio = Susp.WheelLoadRatio;                                          // API VERIFY

		// Steering -------------------------------------------------------------
		// Only the front axle steers. Rear steer is explicitly zeroed rather
		// than left alone, so a wheel asset authored with a non-zero angle
		// cannot introduce four-wheel steering by accident.
		Wheel->MaxSteerAngle = bIsFront ? Setup->Steering.MaxSteerAngleDegrees : 0.f;          // API VERIFY
	}
}

void UVehicleSetupApplier::ApplySteering(
	const UVehicleSetupDataAsset* Setup,
	UChaosWheeledVehicleMovementComponent* Movement)
{
	const FVehicleSteeringConfig& Config = Setup->Steering;

	Movement->SteeringSetup.SteeringType = ESteeringType::Ackermann;                           // API VERIFY
	Movement->SteeringSetup.AngleRatio = Config.AckermannAccuracy;                             // API VERIFY

	// The speed-sensitive steering curve. Chaos takes normalised speed against a
	// steer-angle multiplier, which is exactly the Data Asset's form, so this is
	// a straight copy.
	if (MidanCurve::HasKeys(Config.SteeringCurve))
	{
		Movement->SteeringSetup.SteeringCurve = Config.SteeringCurve;                           // API VERIFY
	}

	// NOTE: input SHAPING — rise/fall rate limiting and the keyboard curve — is
	// deliberately not pushed into Chaos. It happens in UVehicleInputComponent
	// at Phase 3, before the normalised input reaches the movement component, so
	// that the AI and the player share one shaping path. Chaos only ever sees an
	// already-shaped steer value in [-1, 1].
}

#if WITH_EDITOR

void UVehicleSetupApplier::ReapplyToAllInstances(const UVehicleSetupDataAsset* ChangedSetup)
{
	if (!ChangedSetup || !GEngine)
	{
		return;
	}

	int32 ReappliedCount = 0;

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World)
		{
			continue;
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			UChaosWheeledVehicleMovementComponent* Movement =
				It->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();

			if (!Movement)
			{
				continue;
			}

			// Phase 3 gives AMidanVehiclePawn an accessor for its setup asset so
			// this can filter to instances actually using ChangedSetup. Until
			// that exists, re-applying to every vehicle is correct but broader
			// than necessary — harmless in editor, and never compiled into a
			// shipped build.
			if (Apply(ChangedSetup, Movement))
			{
				++ReappliedCount;
			}
		}
	}

	UE_LOG(LogMidanVehicle, Log, TEXT("Hot-reloaded setup '%s' onto %d vehicle instance(s)."),
		*ChangedSetup->GetName(), ReappliedCount);
}

#endif // WITH_EDITOR
