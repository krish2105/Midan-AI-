#include "VehicleInputComponent.h"

#include "Config/VehicleSteeringConfig.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "MidanCurveUtils.h"
#include "MidanInputConfigDataAsset.h"
#include "MidanLogChannels.h"
#include "MidanMathUtils.h"

UVehicleInputComponent::UVehicleInputComponent()
{
	// No tick. Input arrives as events, and shaping is driven by the pawn's
	// existing tick so there is one ordering rather than two competing ones —
	// shaping must run after the frame's input events and before the input is
	// handed to the solver.
	PrimaryComponentTick.bCanEverTick = false;
}

UEnhancedInputLocalPlayerSubsystem* UVehicleInputComponent::GetInputSubsystem() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return nullptr;
	}

	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		// Normal for an AI-controlled vehicle. The AI never uses this component
		// at all — it builds an FMidanVehicleInputState directly — so this is
		// not a warning.
		return nullptr;
	}

	return ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
}

void UVehicleInputComponent::SetupInputBindings(UEnhancedInputComponent* EnhancedInput, const UMidanInputConfigDataAsset* LoadedConfig)
{
	if (!EnhancedInput || !LoadedConfig)
	{
		UE_LOG(LogMidanVehicle, Error,
			TEXT("VehicleInputComponent: cannot bind input — EnhancedInputComponent %s, config %s."),
			EnhancedInput ? TEXT("ok") : TEXT("NULL"),
			LoadedConfig ? TEXT("ok") : TEXT("NULL"));
		return;
	}

	ActiveConfig = LoadedConfig;

	// Push the mapping context. Without it no action fires regardless of the
	// bindings below.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem())
	{
		if (const UInputMappingContext* Context = LoadedConfig->DrivingContext.Get())
		{
			Subsystem->AddMappingContext(Context, LoadedConfig->DrivingContextPriority);
		}
		else
		{
			UE_LOG(LogMidanVehicle, Error,
				TEXT("VehicleInputComponent: DrivingContext is not loaded. The pawn must resolve the "
					 "soft pointer before calling SetupInputBindings."));
		}
	}

	// Bind only what is assigned. A null action is already a validation error on
	// the asset (UMidanInputConfigDataAsset::ValidateMidanData), so here we skip
	// quietly rather than reporting the same problem twice per possession.
	const auto BindAxis = [EnhancedInput, this](const TSoftObjectPtr<UInputAction>& Action, auto Handler)
	{
		if (const UInputAction* Loaded = Action.Get())
		{
			EnhancedInput->BindAction(Loaded, ETriggerEvent::Triggered, this, Handler);
			// Completed matters: without it a released axis keeps its last value
			// and the car drives itself off the track.
			EnhancedInput->BindAction(Loaded, ETriggerEvent::Completed, this, Handler);
		}
	};

	BindAxis(LoadedConfig->ThrottleAction,  &UVehicleInputComponent::OnThrottle);
	BindAxis(LoadedConfig->BrakeAction,     &UVehicleInputComponent::OnBrake);
	BindAxis(LoadedConfig->HandbrakeAction, &UVehicleInputComponent::OnHandbrake);

	// Steer binds Completed to a dedicated handler so the digital hold timer is
	// reset on release. Routing release through OnSteer would leave
	// DigitalSteerHeldSeconds running and the keyboard ramp would start
	// mid-curve on the next press.
	if (const UInputAction* Steer = LoadedConfig->SteerAction.Get())
	{
		EnhancedInput->BindAction(Steer, ETriggerEvent::Triggered, this, &UVehicleInputComponent::OnSteer);
		EnhancedInput->BindAction(Steer, ETriggerEvent::Completed, this, &UVehicleInputComponent::OnSteerReleased);
	}

	if (const UInputAction* ShiftUp = LoadedConfig->ShiftUpAction.Get())
	{
		EnhancedInput->BindAction(ShiftUp, ETriggerEvent::Started, this, &UVehicleInputComponent::OnShiftUp);
	}

	if (const UInputAction* ShiftDown = LoadedConfig->ShiftDownAction.Get())
	{
		EnhancedInput->BindAction(ShiftDown, ETriggerEvent::Started, this, &UVehicleInputComponent::OnShiftDown);
	}

	UE_LOG(LogMidanVehicle, Log, TEXT("VehicleInputComponent: input bound for '%s'."), *GetNameSafe(GetOwner()));
}

void UVehicleInputComponent::TeardownInputBindings()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem())
	{
		if (ActiveConfig)
		{
			if (const UInputMappingContext* Context = ActiveConfig->DrivingContext.Get())
			{
				Subsystem->RemoveMappingContext(Context);
			}
		}
	}

	ActiveConfig = nullptr;

	// Zero the raw state. An unpossessed car holding a stale throttle value
	// would keep accelerating if anything later re-applied its input.
	RawThrottle = 0.f;
	RawBrake = 0.f;
	RawSteer = 0.f;
	RawHandbrake = 0.f;
	bShiftUpPending = false;
	bShiftDownPending = false;
	SmoothedSteer = 0.f;
	DigitalSteerHeldSeconds = 0.f;
	DigitalSteerDirection = 0.f;
	bDigitalSteerActive = false;
	ShapedInput = FMidanVehicleInputState();
}

void UVehicleInputComponent::OnThrottle(const FInputActionValue& Value)
{
	RawThrottle = FMath::Clamp(Value.Get<float>(), 0.f, 1.f);
}

void UVehicleInputComponent::OnBrake(const FInputActionValue& Value)
{
	RawBrake = FMath::Clamp(Value.Get<float>(), 0.f, 1.f);
}

void UVehicleInputComponent::OnSteer(const FInputActionValue& Value)
{
	RawSteer = FMath::Clamp(Value.Get<float>(), -1.f, 1.f);
}

void UVehicleInputComponent::OnSteerReleased(const FInputActionValue& Value)
{
	RawSteer = 0.f;

	// Reset the digital ramp so the next press starts from the beginning of the
	// keyboard curve rather than resuming where it left off.
	DigitalSteerHeldSeconds = 0.f;
	DigitalSteerDirection = 0.f;
}

void UVehicleInputComponent::OnHandbrake(const FInputActionValue& Value)
{
	RawHandbrake = FMath::Clamp(Value.Get<float>(), 0.f, 1.f);
}

void UVehicleInputComponent::OnShiftUp(const FInputActionValue& Value)
{
	bShiftUpPending = true;
}

void UVehicleInputComponent::OnShiftDown(const FInputActionValue& Value)
{
	bShiftDownPending = true;
}

float UVehicleInputComponent::EvaluateSteeringLimit(const FVehicleSteeringConfig& Config, float ForwardSpeedKmh)
{
	// Fallback of 1.0 means "no limiting" when the curve is unauthored. That is
	// deliberate: an unset curve is already a validation ERROR on the setup
	// asset, and silently limiting to some invented value would make an
	// unauthored car feel subtly wrong instead of obviously wrong.
	const float Limit = MidanCurve::EvalSafe(Config.SteeringCurve, FMath::Abs(ForwardSpeedKmh), 1.f);
	return FMath::Clamp(Limit, 0.f, 1.f);
}

float UVehicleInputComponent::ShapeDigitalSteer(const FVehicleSteeringConfig& Config, float DeltaSeconds)
{
	const float Direction = FMath::Sign(RawSteer);

	// Direction change restarts the ramp. A left-to-right flick should not
	// inherit the hold time from the previous direction — that would snap the
	// wheel across instantly, which is exactly the feel this curve exists to
	// prevent.
	if (!FMath::IsNearlyEqual(Direction, DigitalSteerDirection))
	{
		DigitalSteerDirection = Direction;
		DigitalSteerHeldSeconds = 0.f;
	}

	DigitalSteerHeldSeconds += DeltaSeconds;

	// Fallback of 1.0 keeps an unauthored keyboard curve behaving as raw digital
	// input rather than disabling steering entirely.
	const float Magnitude = FMath::Clamp(
		MidanCurve::EvalSafe(Config.KeyboardShapingCurve, DigitalSteerHeldSeconds, 1.f), 0.f, 1.f);

	return Direction * Magnitude;
}

const FMidanVehicleInputState& UVehicleInputComponent::ShapeInput(
	const FVehicleSteeringConfig& SteeringConfig,
	const float ForwardSpeedKmh,
	const float DeltaSeconds)
{
	// --- Throttle and brake pass through. Powertrain response shaping is the
	//     torque curve's job, not the input layer's — shaping throttle here
	//     would double up with the torque curve and make both untunable.
	ShapedInput.Throttle = RawThrottle;
	ShapedInput.Brake = RawBrake;
	ShapedInput.Handbrake = RawHandbrake;

	// --- Steering. Three stages, in this order, and the order matters.

	// Stage 1: device classification. A keyboard reports full deflection on the
	// first frame; a stick ramps. Threshold comes from the input config.
	const float DigitalThreshold = ActiveConfig ? ActiveConfig->DigitalDetectionThreshold : 0.999f;
	const bool bLooksDigital = FMath::Abs(RawSteer) >= DigitalThreshold;

	float TargetSteer = 0.f;
	if (bLooksDigital)
	{
		bDigitalSteerActive = true;
		TargetSteer = ShapeDigitalSteer(SteeringConfig, DeltaSeconds);
	}
	else
	{
		bDigitalSteerActive = false;
		DigitalSteerHeldSeconds = 0.f;
		DigitalSteerDirection = 0.f;

		// Analogue deadzone, rescaled so the usable range still reaches 1.0.
		// Without rescaling, a deadzone silently caps maximum steer at
		// (1 - deadzone), which reads as the car refusing to take full lock.
		const float AbsRaw = FMath::Abs(RawSteer);
		const float Deadzone = FMath::Clamp(SteeringConfig.AnalogueDeadzone, 0.f, 0.5f);
		if (AbsRaw > Deadzone)
		{
			const float Rescaled = MidanMath::MapRangeClamped(AbsRaw, Deadzone, 1.f, 0.f, 1.f);
			TargetSteer = FMath::Sign(RawSteer) * Rescaled;
		}
	}

	// Stage 2: the mandatory speed-sensitive limit (master prompt §1.2).
	TargetSteer *= EvaluateSteeringLimit(SteeringConfig, ForwardSpeedKmh);

	// Stage 3: asymmetric rate limiting. Applied LAST so the limiter smooths the
	// final commanded angle. Applying it before the speed limit would let a
	// deceleration produce a step change in steer angle as the limit widened.
	SmoothedSteer = MidanMath::RateLimitedApproach(
		SmoothedSteer,
		TargetSteer,
		SteeringConfig.SteeringInputRiseRate,
		SteeringConfig.SteeringInputFallRate,
		DeltaSeconds);

	ShapedInput.Steer = SmoothedSteer;

	// --- Gear change edges, consumed here so a press is delivered exactly once.
	ShapedInput.bShiftUp = bShiftUpPending;
	ShapedInput.bShiftDown = bShiftDownPending;
	bShiftUpPending = false;
	bShiftDownPending = false;

	ShapedInput.Sanitise();
	return ShapedInput;
}
