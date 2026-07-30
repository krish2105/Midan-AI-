#include "MidanVehicleMovementComponent.h"

#include "MidanMathUtils.h"
#include "VehicleAssistComponent.h"

UMidanVehicleMovementComponent::UMidanVehicleMovementComponent()
{
	// Ticks on the game thread only to finite-difference acceleration for the
	// HUD and telemetry. All physics work is in the solver and the async
	// callbacks — see docs/ARCHITECTURE.md §2.3.
	PrimaryComponentTick.bCanEverTick = true;

	// NO tuning values are set here. Mass, torque curve, gear ratios, suspension,
	// tyre friction, steering and differential all come from
	// UVehicleSetupDataAsset via UVehicleSetupApplier. Setting a default here
	// would create a second source of truth and silently win whenever the
	// applier failed — which is precisely the failure mode CLAUDE.md's
	// no-hardcoded-tuning rule exists to prevent.
}

void UMidanVehicleMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	PreviousVelocity = GetOwner() ? GetOwner()->GetVelocity() : FVector::ZeroVector;
}

void UMidanVehicleMovementComponent::SetMidanInput(const FMidanVehicleInputState& Input)
{
	CommandedInput = Input;
	EffectiveInput = Input;

	// Assists modify the input; they never apply force. Their output goes through
	// the same solver entry as unassisted input, so there is exactly one path
	// into the physics (master prompt §4.2).
	if (UVehicleAssistComponent* AssistComp = Assists.Get())
	{
		AssistComp->ApplyAssists(EffectiveInput, GetForwardSpeedKmh(), GetChassisSlipAngleDegrees());
	}

	// API VERIFY: the Chaos input setters. SetThrottleInput / SetBrakeInput /
	// SetSteeringInput / SetHandbrakeInput are the documented UE5 names on
	// UChaosVehicleMovementComponent; confirm the handbrake setter takes a float
	// rather than a bool on 5.8, and confirm gear change is
	// IncreaseGear/DecreaseGear vs SetTargetGear. See docs/ASSUMPTIONS.md A21.
	SetThrottleInput(EffectiveInput.Throttle);
	SetBrakeInput(EffectiveInput.Brake);
	SetSteeringInput(EffectiveInput.Steer);
	SetHandbrakeInput(EffectiveInput.Handbrake > 0.5f);

	// Gear changes are edge-triggered. The input state's flags are consumed once
	// by whoever produced them, so a single press cannot double-shift.
	if (EffectiveInput.bShiftUp)
	{
		IncreaseGear();
	}
	else if (EffectiveInput.bShiftDown)
	{
		DecreaseGear();
	}
}

float UMidanVehicleMovementComponent::GetForwardSpeedKmh() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 0.f;
	}

	// Signed along the chassis forward axis, so reverse reads negative. Using
	// velocity magnitude would make reverse look like forward motion to the HUD
	// and to the AI's longitudinal controller.
	const FVector Forward = Owner->GetActorForwardVector();
	const float ForwardCmS = FVector::DotProduct(Owner->GetVelocity(), Forward);
	return ForwardCmS * MidanMath::CmSToKmH;
}

float UMidanVehicleMovementComponent::GetChassisSlipAngleDegrees() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 0.f;
	}

	const FVector Velocity = Owner->GetVelocity();

	// Project onto the horizontal plane. Vertical velocity from a jump or a crest
	// is not slip, and including it would report a large slip angle mid-air where
	// the concept is meaningless.
	const FVector VelocityFlat(Velocity.X, Velocity.Y, 0.f);
	const float SpeedFlat = VelocityFlat.Size();

	// Below walking pace the angle is numerically unstable and perceptually
	// irrelevant — a stationary car has no direction of travel to differ from.
	static constexpr float MinSpeedForSlipCmS = 100.f;
	if (SpeedFlat < MinSpeedForSlipCmS)
	{
		return 0.f;
	}

	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Owner->GetActorRightVector().GetSafeNormal2D();

	const FVector VelocityDir = VelocityFlat / SpeedFlat;
	const float ForwardDot = FVector::DotProduct(VelocityDir, Forward);
	const float RightDot = FVector::DotProduct(VelocityDir, Right);

	// Atan2 rather than acos: it gives a signed angle, and the sign is what tells
	// the camera which way to yaw so the drift reads correctly on screen.
	return FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
}

void UMidanVehicleMovementComponent::GetAccelerationG(float& OutLateralG, float& OutLongitudinalG) const
{
	OutLateralG = CachedLateralG;
	OutLongitudinalG = CachedLongitudinalG;
}

void UMidanVehicleMovementComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	if (!Owner || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Finite-difference acceleration in the chassis frame. Done on the game
	// thread rather than in the physics callback because the only consumers are
	// the HUD and telemetry, both of which read at frame rate or slower — there
	// is no reason to pay for it 120 times a second.
	const FVector Velocity = Owner->GetVelocity();
	const FVector AccelCmS2 = (Velocity - PreviousVelocity) / DeltaTime;
	PreviousVelocity = Velocity;

	const FVector Forward = Owner->GetActorForwardVector();
	const FVector Right = Owner->GetActorRightVector();

	CachedLongitudinalG = FVector::DotProduct(AccelCmS2, Forward) / MidanMath::GravityCmS2;
	CachedLateralG = FVector::DotProduct(AccelCmS2, Right) / MidanMath::GravityCmS2;
}

void UMidanVehicleMovementComponent::WriteWheelPhysicsToFrameState(FMidanVehicleFrameState& OutState) const
{
	const int32 WheelCount = FMath::Min(Wheels.Num(), MidanVehicleConstants::NumWheels);

	for (int32 i = 0; i < WheelCount; ++i)
	{
		// API VERIFY: FWheelStatus field names on 5.8 — LongitudinalSlip,
		// LateralSlip (or SlipAngle), NormalizedSuspensionLength, SpringForce,
		// bInContact. See docs/ASSUMPTIONS.md A21.
		const FWheelStatus& Status = GetWheelState(i);

		FMidanWheelState& Wheel = OutState.Wheels[i];
		Wheel.SlipRatio = Status.LongitudinalSlip;
		Wheel.SlipAngleDegrees = Status.LateralSlip;
		Wheel.NormalLoadN = Status.SpringForce;
		Wheel.AngularVelocity = Wheels[i] ? Wheels[i]->GetRotationAngularVelocity() : 0.f;
		// bInContact and SuspensionCompression are written by the surface sensor,
		// which owns contact state. Not duplicated here — two writers for one
		// field is how they end up disagreeing.
	}
}
