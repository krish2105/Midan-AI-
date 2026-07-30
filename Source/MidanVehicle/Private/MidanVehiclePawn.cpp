#include "MidanVehiclePawn.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "EnhancedInputComponent.h"
#include "MidanDeveloperSettings.h"
#include "MidanInputConfigDataAsset.h"
#include "MidanLogChannels.h"
#include "MidanChaseCameraComponent.h"
#include "MidanVehicleMovementComponent.h"
#include "SurfaceResponseDataAsset.h"
#include "VehicleAeroComponent.h"
#include "VehicleAssistComponent.h"
#include "VehicleAudioComponent.h"
#include "VehicleFXComponent.h"
#include "VehicleFeelDataAsset.h"
#include "VehicleHapticsComponent.h"
#include "VehicleInputComponent.h"
#include "VehicleSetupApplier.h"
#include "VehicleSetupDataAsset.h"
#include "VehicleSurfaceSensorComponent.h"

AMidanVehiclePawn::AMidanVehiclePawn()
{
	// Ticks to shape player input and feed the solver. Justified in
	// docs/ARCHITECTURE.md §2.3: input arrives as frame events and the steering
	// rate limiter is frame-dt based, so shaping belongs on the game thread. The
	// physics work is in the async callbacks.
	PrimaryActorTick.bCanEverTick = true;

	// Replace the base class's movement component with ours, so the per-substep
	// assist hook exists. Same name so the base class's internal wiring still
	// finds it.
	MidanMovement = CreateDefaultSubobject<UMidanVehicleMovementComponent>(TEXT("VehicleMovementComp"));
	MidanMovement->SetIsReplicated(false); // Single-player slice; networking is a non-goal.

	Aero = CreateDefaultSubobject<UVehicleAeroComponent>(TEXT("AeroComponent"));
	SurfaceSensor = CreateDefaultSubobject<UVehicleSurfaceSensorComponent>(TEXT("SurfaceSensorComponent"));
	Assists = CreateDefaultSubobject<UVehicleAssistComponent>(TEXT("AssistComponent"));
	MidanInput = CreateDefaultSubobject<UVehicleInputComponent>(TEXT("MidanInputComponent"));

	// Phase 4 feel layer. The camera attaches to the root rather than to the
	// mesh: attaching to a skeletal mesh would inherit per-bone animation, and
	// suspension bone motion reaching the camera is exactly the chatter
	// FMidanCameraModeConfig::VerticalDamping exists to filter.
	ChaseCamera = CreateDefaultSubobject<UMidanChaseCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(RootComponent);

	VehicleAudio = CreateDefaultSubobject<UVehicleAudioComponent>(TEXT("VehicleAudio"));
	VehicleFX = CreateDefaultSubobject<UVehicleFXComponent>(TEXT("VehicleFX"));
	Haptics = CreateDefaultSubobject<UVehicleHapticsComponent>(TEXT("Haptics"));

	// NO asset resolution here. CLAUDE.md forbids FindObject, LoadObject and hard
	// references in constructors — a constructor runs during CDO creation, so a
	// hard reference would drag the asset and its whole dependency chain into
	// memory before the first map loads.
}

void AMidanVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	// Assists need to be reachable from the movement component's per-substep path.
	if (MidanMovement && Assists)
	{
		MidanMovement->SetAssistComponent(Assists);
	}

	// Locked until the setup asset has been applied. A car with engine defaults
	// is not this car, and letting it be driven for the two frames before the
	// async load completes would produce a first-corner feel nobody authored.
	bInputLocked = true;

	if (TelemetrySourceId.IsNone())
	{
		TelemetrySourceId = GetFName();
	}

	if (const UMidanDeveloperSettings* Settings = UMidanDeveloperSettings::Get())
	{
		bTelemetryCaptureEnabled = Settings->bTelemetryCaptureEnabledByDefault;
	}

	RequestSetupLoad();
}

void AMidanVehiclePawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Cancel an in-flight load. Without this, a pawn destroyed during streaming
	// completes into a dangling this.
	if (SetupLoadHandle.IsValid())
	{
		SetupLoadHandle->CancelHandle();
		SetupLoadHandle.Reset();
	}
	if (FeelLoadHandle.IsValid())
	{
		FeelLoadHandle->CancelHandle();
		FeelLoadHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void AMidanVehiclePawn::RequestSetupLoad()
{
	// Fall back to the project default when a pawn is placed without a setup —
	// which happens constantly during Phase 3 testing in an empty level.
	TSoftObjectPtr<UVehicleSetupDataAsset> SetupToLoad = VehicleSetup;
	TSoftObjectPtr<USurfaceResponseDataAsset> SurfaceToLoad = SurfaceResponse;

	if (const UMidanDeveloperSettings* Settings = UMidanDeveloperSettings::Get())
	{
		if (SetupToLoad.IsNull())
		{
			SetupToLoad = Settings->FallbackVehicleSetup;
		}
		if (SurfaceToLoad.IsNull())
		{
			SurfaceToLoad = Settings->DefaultSurfaceResponse;
		}
	}

	if (SetupToLoad.IsNull())
	{
		UE_LOG(LogMidanVehicle, Error,
			TEXT("MidanVehiclePawn '%s': no VehicleSetup and no project fallback. The car will run on "
				 "engine defaults and its handling will be meaningless. Set VehicleSetup on the "
				 "Blueprint, or FallbackVehicleSetup in Project Settings > Midan."),
			*GetName());
		return;
	}

	TArray<FSoftObjectPath> ToLoad;
	ToLoad.Add(SetupToLoad.ToSoftObjectPath());

	if (!SurfaceToLoad.IsNull())
	{
		ToLoad.Add(SurfaceToLoad.ToSoftObjectPath());
	}

	// Only the player needs an input config. An AI pawn legitimately has none.
	if (!InputConfig.IsNull())
	{
		ToLoad.Add(InputConfig.ToSoftObjectPath());
	}

	// The feel asset is NOT requested here: it is referenced by the setup asset,
	// so its path is unknown until that has loaded. RequestFeelLoad handles it.

	// Store the resolved soft pointers so OnSetupLoaded reads the same ones the
	// load was issued for, rather than re-resolving and possibly picking up a
	// different fallback.
	VehicleSetup = SetupToLoad;
	SurfaceResponse = SurfaceToLoad;

	SetupLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		ToLoad,
		FStreamableDelegate::CreateUObject(this, &AMidanVehiclePawn::OnSetupLoaded));
}

void AMidanVehiclePawn::OnSetupLoaded()
{
	LoadedSetup = VehicleSetup.Get();
	LoadedSurfaceResponse = SurfaceResponse.Get();
	LoadedInputConfig = InputConfig.Get();

	if (!LoadedSetup)
	{
		UE_LOG(LogMidanVehicle, Error,
			TEXT("MidanVehiclePawn '%s': setup asset failed to load. Car remains on engine defaults."),
			*GetName());
		return;
	}

	// The feel asset lives behind a soft pointer INSIDE the setup asset, so its
	// path only becomes knowable now. Chain a second request rather than
	// applying with a null feel asset.
	RequestFeelLoad();
}

void AMidanVehiclePawn::RequestFeelLoad()
{
	if (!LoadedSetup || LoadedSetup->Feel.IsNull())
	{
		// No feel asset authored. Apply the physics anyway — the car drives, it
		// just has no camera behaviour or audio. ApplyLoadedSetup logs that.
		ApplyLoadedSetup();
		return;
	}

	if (LoadedSetup->Feel.Get())
	{
		// Already resident, typically because another vehicle of the same class
		// loaded it first. Skip the round trip.
		OnFeelLoaded();
		return;
	}

	FeelLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		LoadedSetup->Feel.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &AMidanVehiclePawn::OnFeelLoaded));
}

void AMidanVehiclePawn::OnFeelLoaded()
{
	ApplyLoadedSetup();
}

void AMidanVehiclePawn::ApplyLoadedSetup()
{
	if (!LoadedSetup || !MidanMovement)
	{
		return;
	}

	// Validate before applying. An invalid setup reaching the solver produces NaN
	// and surfaces as the car silently vanishing — a symptom that gives no clue
	// about its cause. Validating here converts that into a named error.
	FMidanValidationResult Validation;
	if (!LoadedSetup->IsMidanDataValid(Validation))
	{
		UE_LOG(LogMidanVehicle, Error,
			TEXT("MidanVehiclePawn '%s': setup '%s' has %d validation error(s). NOT applying — a car "
				 "built from invalid data produces NaN in the solver and vanishes without explanation."),
			*GetName(), *GetNameSafe(LoadedSetup), Validation.CountErrors());

		for (const FMidanValidationIssue& Issue : Validation.Issues)
		{
			if (Issue.bIsError)
			{
				UE_LOG(LogMidanVehicle, Error, TEXT("  [%s] %s"), *Issue.Context.ToString(), *Issue.Message.ToString());
			}
		}
		return;
	}

	// Warnings are expected and informative — the rally car's high centre of mass
	// trips several deliberately.
	for (const FMidanValidationIssue& Issue : Validation.Issues)
	{
		if (!Issue.bIsError)
		{
			UE_LOG(LogMidanVehicle, Verbose, TEXT("Setup '%s' [%s]: %s"),
				*GetNameSafe(LoadedSetup), *Issue.Context.ToString(), *Issue.Message.ToString());
		}
	}

	// Data Asset -> Chaos component. One direction only: the asset is the source
	// of truth and the movement component is a downstream consumer.
	if (!UVehicleSetupApplier::Apply(LoadedSetup, MidanMovement))
	{
		UE_LOG(LogMidanVehicle, Error, TEXT("MidanVehiclePawn '%s': UVehicleSetupApplier::Apply failed."), *GetName());
		return;
	}

	// Aero needs the chassis body to apply force to, and the mass to clamp
	// against.
	if (Aero)
	{
		Aero->InitialiseFromConfig(LoadedSetup->Aero, GetMesh(), LoadedSetup->Mass.MassKg);
	}

	if (SurfaceSensor)
	{
		SurfaceSensor->InitialiseFromAsset(MidanMovement, LoadedSurfaceResponse);
	}

	if (Assists)
	{
		Assists->InitialiseFromConfig(LoadedSetup->Assists, MidanMovement);
	}

	// --- Phase 4 feel layer. Initialised after the physics components so the
	// camera and audio read a configured car rather than engine defaults.
	LoadedFeel = LoadedSetup->Feel.Get();

	if (!LoadedFeel)
	{
		// Not fatal — the car drives. But it drives with a fixed-FOV unlagged
		// camera and no sound, which will read as "the game feels bad" rather
		// than "an asset is missing", so it is logged loudly.
		UE_LOG(LogMidanVehicle, Warning,
			TEXT("MidanVehiclePawn '%s': setup '%s' has no loaded Feel asset. Camera, audio, FX and "
				 "haptics will all be inert. The car will drive correctly and feel wrong."),
			*GetName(), *GetNameSafe(LoadedSetup));
	}
	else
	{
		if (ChaseCamera)
		{
			ChaseCamera->InitialiseFromAsset(LoadedFeel, MidanMovement, SurfaceSensor);
		}
		if (VehicleAudio)
		{
			VehicleAudio->InitialiseFromAsset(
				LoadedFeel, MidanMovement, SurfaceSensor, LoadedSetup->Powertrain.MaxRPM);
		}
		if (VehicleFX)
		{
			VehicleFX->InitialiseFromAsset(LoadedFeel, MidanMovement, SurfaceSensor);
		}
		if (Haptics)
		{
			Haptics->InitialiseFromAsset(LoadedFeel, MidanMovement, SurfaceSensor);
		}
	}

	bSetupApplied = true;

	// Unlock only once everything is configured. The race game mode re-locks for
	// the grid and countdown; this unlock exists so a car spawned outside a race
	// (Phase 3 testing) is driveable.
	bInputLocked = false;

	UE_LOG(LogMidanVehicle, Log, TEXT("MidanVehiclePawn '%s': applied setup '%s' (%s)."),
		*GetName(), *GetNameSafe(LoadedSetup), *LoadedSetup->VehicleClass.ToString());
}

void AMidanVehiclePawn::ApplyInput(const FMidanVehicleInputState& Input)
{
	// THE ONLY DOOR. Player and AI both arrive here.
	if (!MidanMovement)
	{
		return;
	}

	if (bInputLocked || !bSetupApplied)
	{
		// Zero throttle AND hold the brake. Merely dropping the input would let a
		// car roll on a sloped grid before the lights, which players notice
		// immediately.
		FMidanVehicleInputState Locked;
		Locked.Brake = 1.f;
		LastAppliedInput = Locked;
		MidanMovement->SetMidanInput(Locked);
		return;
	}

	LastAppliedInput = Input;
	LastAppliedInput.Sanitise();
	MidanMovement->SetMidanInput(LastAppliedInput);
}

void AMidanVehiclePawn::SetInputLocked(const bool bLocked)
{
	bInputLocked = bLocked;

	// Apply immediately so the lock takes effect this frame rather than next.
	// A one-frame delay on the countdown release is a false start.
	if (bLocked)
	{
		ApplyInput(FMidanVehicleInputState());
	}
}

void AMidanVehiclePawn::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Player input shaping. An AI pawn has no bound input config, so this does
	// nothing for opponents — and crucially the AI's own ApplyInput call is not
	// affected, because it goes through the same public method from its
	// controller's tick.
	if (MidanInput && LoadedInputConfig && LoadedSetup && IsPlayerControlled())
	{
		const FMidanVehicleInputState& Shaped =
			MidanInput->ShapeInput(LoadedSetup->Steering, GetForwardSpeedKmh(), DeltaSeconds);
		ApplyInput(Shaped);

		// Look-ahead needs the driver's steering. Pushed rather than pulled: a
		// camera that could reach into the input component could also change
		// what it observes.
		if (ChaseCamera)
		{
			ChaseCamera->SetSteerInput(Shaped.Steer);
		}
	}
}

void AMidanVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogMidanVehicle, Error,
			TEXT("MidanVehiclePawn '%s': input component is not a UEnhancedInputComponent. "
				 "Check DefaultInputComponentClass in Config/DefaultInput.ini."),
			*GetName());
		return;
	}

	// The config may not have loaded yet — possession can precede the async load.
	// When that happens, ApplyLoadedSetup does not re-bind, so binding is retried
	// from the next possession or the config is already present. For the common
	// case (level-placed pawn, possessed on BeginPlay) the load completes first.
	if (MidanInput && LoadedInputConfig)
	{
		MidanInput->SetupInputBindings(EnhancedInput, LoadedInputConfig);
	}
	else if (MidanInput && !InputConfig.IsNull())
	{
		UE_LOG(LogMidanVehicle, Warning,
			TEXT("MidanVehiclePawn '%s': possessed before the input config finished loading. "
				 "Input will bind once loading completes."),
			*GetName());
	}
}

void AMidanVehiclePawn::UnPossessed()
{
	if (MidanInput)
	{
		MidanInput->TeardownInputBindings();
	}

	// Stop the car. An unpossessed vehicle holding its last throttle value would
	// keep accelerating away.
	SetInputLocked(true);

	Super::UnPossessed();
}

float AMidanVehiclePawn::GetForwardSpeedKmh() const
{
	return MidanMovement ? MidanMovement->GetForwardSpeedKmh() : 0.f;
}

FGameplayTag AMidanVehiclePawn::GetVehicleClassTag() const
{
	return LoadedSetup ? LoadedSetup->VehicleClass : FGameplayTag();
}

void AMidanVehiclePawn::GetVehicleFrameState(FMidanVehicleFrameState& OutState) const
{
	OutState.Transform = GetActorTransform();
	OutState.LinearVelocity = GetVelocity();

	if (const UPrimitiveComponent* Body = GetMesh())
	{
		OutState.AngularVelocity = Body->GetPhysicsAngularVelocityInRadians();
	}

	if (MidanMovement)
	{
		OutState.ForwardSpeedKmh = MidanMovement->GetForwardSpeedKmh();
		OutState.ChassisSlipAngleDegrees = MidanMovement->GetChassisSlipAngleDegrees();
		MidanMovement->GetAccelerationG(OutState.LateralG, OutState.LongitudinalG);

		// API VERIFY: GetEngineRotationSpeed and GetCurrentGear names on 5.8 —
		// docs/ASSUMPTIONS.md A21.
		OutState.EngineRPM = MidanMovement->GetEngineRotationSpeed();
		OutState.Gear = MidanMovement->GetCurrentGear();

		MidanMovement->WriteWheelPhysicsToFrameState(OutState);
	}

	// Surface fields last: the sensor owns contact state and compression, and
	// writing them after the movement component avoids two writers for one field.
	if (SurfaceSensor)
	{
		SurfaceSensor->WriteToFrameState(OutState);
	}
}

void AMidanVehiclePawn::CaptureTelemetryState(FMidanVehicleFrameState& OutState) const
{
	GetVehicleFrameState(OutState);
}

void AMidanVehiclePawn::CaptureTelemetryInput(FMidanVehicleInputState& OutInput) const
{
	OutInput = LastAppliedInput;
}

void AMidanVehiclePawn::NotifyHit(
	UPrimitiveComponent* MyComp,
	AActor* Other,
	UPrimitiveComponent* OtherComp,
	const bool bSelfMoved,
	const FVector HitLocation,
	const FVector HitNormal,
	const FVector NormalImpulse,
	const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	// ONE impulse, THREE channels. Camera shake, impact audio and haptics all
	// read the same number from the same event, which is what stops a collision
	// that looks minor from sounding severe or feeling wrong. Three channels
	// disagreeing about the same event is worse than any one being absent.
	const float Impulse = NormalImpulse.Size();

	// Ignore the continuous micro-contacts of a car resting on the road. Without
	// a floor, a stationary vehicle reports a stream of tiny hits and the
	// controller buzzes on the grid.
	static constexpr float MinReportableImpulse = 200.f;
	if (Impulse < MinReportableImpulse)
	{
		return;
	}

	if (ChaseCamera)  { ChaseCamera->ReportImpact(Impulse); }
	if (VehicleAudio) { VehicleAudio->ReportImpact(Impulse); }
	if (Haptics)      { Haptics->ReportImpact(Impulse); }
}
