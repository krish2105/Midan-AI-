#include "MidanChaseCameraComponent.h"

#include "MidanCurveUtils.h"
#include "MidanGameplayTags.h"
#include "MidanLogChannels.h"
#include "MidanMathUtils.h"
#include "MidanVehicleMovementComponent.h"
#include "VehicleFeelDataAsset.h"
#include "VehicleSurfaceSensorComponent.h"

UMidanChaseCameraComponent::UMidanChaseCameraComponent()
{
	// Per-frame. A camera is a render-rate concern by definition, and every
	// behaviour here is defined against frame delta.
	PrimaryComponentTick.bCanEverTick = true;

	// Post-processing is driven per frame from the feel curves, so the component
	// must own its settings rather than inherit the volume's.
	bUsePawnControlRotation = false;

	CurrentModeTag = MidanTags::Vehicle_Camera_ChaseFar;
}

void UMidanChaseCameraComponent::InitialiseFromAsset(
	const UVehicleFeelDataAsset* InFeel,
	UMidanVehicleMovementComponent* InMovement,
	UVehicleSurfaceSensorComponent* InSurfaceSensor)
{
	Feel = InFeel;
	Movement = InMovement;
	SurfaceSensor = InSurfaceSensor;

	bInitialised = (Feel != nullptr) && (Movement != nullptr);

	if (!bInitialised)
	{
		UE_LOG(LogMidanVehicle, Error,
			TEXT("MidanChaseCameraComponent on '%s': not initialised (feel %s, movement %s). "
				 "The camera will hold its default view and none of the seven behaviours will run."),
			*GetNameSafe(GetOwner()),
			InFeel ? TEXT("ok") : TEXT("NULL"),
			InMovement ? TEXT("ok") : TEXT("NULL"));
		return;
	}

	// Reset so the next tick snaps rather than lagging in from wherever the
	// camera happened to be. Re-initialising mid-race (a vehicle swap) would
	// otherwise show the camera flying across the level.
	ViewState = FMidanCameraViewState();
}

const FMidanCameraModeConfig& UMidanChaseCameraComponent::GetActiveConfig() const
{
	check(Feel);

	if (CurrentModeTag == MidanTags::Vehicle_Camera_ChaseNear) { return Feel->ChaseNear; }
	if (CurrentModeTag == MidanTags::Vehicle_Camera_Bonnet)    { return Feel->Bonnet; }
	if (CurrentModeTag == MidanTags::Vehicle_Camera_Cockpit)   { return Feel->Cockpit; }
	return Feel->ChaseFar;
}

const IMidanVehicleCameraMode& UMidanChaseCameraComponent::GetActiveMode() const
{
	// Bonnet and cockpit are rigid; the two chase modes are on a boom. The mode
	// tag selects behaviour, the config selects the numbers.
	const bool bRigid =
		CurrentModeTag == MidanTags::Vehicle_Camera_Bonnet ||
		CurrentModeTag == MidanTags::Vehicle_Camera_Cockpit;

	return bRigid
		? static_cast<const IMidanVehicleCameraMode&>(RigidMode)
		: static_cast<const IMidanVehicleCameraMode&>(BoomMode);
}

void UMidanChaseCameraComponent::CycleCameraMode()
{
	if (CurrentModeTag == MidanTags::Vehicle_Camera_ChaseFar)
	{
		SetCameraMode(MidanTags::Vehicle_Camera_ChaseNear);
	}
	else if (CurrentModeTag == MidanTags::Vehicle_Camera_ChaseNear)
	{
		SetCameraMode(MidanTags::Vehicle_Camera_Bonnet);
	}
	else if (CurrentModeTag == MidanTags::Vehicle_Camera_Bonnet)
	{
		SetCameraMode(MidanTags::Vehicle_Camera_Cockpit);
	}
	else
	{
		SetCameraMode(MidanTags::Vehicle_Camera_ChaseFar);
	}
}

void UMidanChaseCameraComponent::SetCameraMode(const FGameplayTag ModeTag)
{
	if (ModeTag == CurrentModeTag)
	{
		return;
	}

	const bool bWasRigid =
		CurrentModeTag == MidanTags::Vehicle_Camera_Bonnet ||
		CurrentModeTag == MidanTags::Vehicle_Camera_Cockpit;
	const bool bNowRigid =
		ModeTag == MidanTags::Vehicle_Camera_Bonnet ||
		ModeTag == MidanTags::Vehicle_Camera_Cockpit;

	CurrentModeTag = ModeTag;

	// Switching BETWEEN behaviour classes teleports the camera — a boom position
	// several metres behind the car is not a sensible starting point for a
	// bonnet view, and damping toward it would sweep the camera through the
	// bodywork. Switching within a class (far <-> near) keeps state, so it
	// glides.
	if (bWasRigid != bNowRigid)
	{
		ViewState.bInitialised = false;
	}
}

void UMidanChaseCameraComponent::ReportImpact(const float ImpulseMagnitude)
{
	// Max, not sum. Several small contacts in one frame should not add up to one
	// large shake, which is how a scrape along a barrier turns into a crash.
	PendingImpactImpulse = FMath::Max(PendingImpactImpulse, FMath::Abs(ImpulseMagnitude));
}

void UMidanChaseCameraComponent::SetPhotoModeEnabled(const bool bEnabled)
{
	bPhotoMode = bEnabled;

	if (bEnabled)
	{
		// Clear shake immediately. A photo mode that still shakes is not a photo
		// mode, and every screenshot in the portfolio depends on this
		// (ART_DIRECTION §9).
		ViewState.ShakeAmplitudeDegrees = 0.f;
		ViewState.RumbleAmplitudeDegrees = 0.f;
		PendingImpactImpulse = 0.f;
	}
}

void UMidanChaseCameraComponent::BuildViewInput(FMidanCameraViewInput& OutInput, const float DeltaSeconds) const
{
	const AActor* Owner = GetOwner();
	OutInput.VehicleTransform = Owner ? Owner->GetActorTransform() : FTransform::Identity;

	if (Movement)
	{
		OutInput.ForwardSpeedKmh = Movement->GetForwardSpeedKmh();
		OutInput.ChassisSlipAngleDegrees = Movement->GetChassisSlipAngleDegrees();
	}

	// Every speed-driven curve in the feel asset shares this normalisation, so
	// they can all be authored against a common 0..1 X axis.
	const float NormaliseKmh = Feel ? FMath::Max(Feel->SpeedNormalisationKmh, 1.f) : 320.f;
	OutInput.NormalisedSpeed = FMath::Clamp(FMath::Abs(OutInput.ForwardSpeedKmh) / NormaliseKmh, 0.f, 1.f);

	OutInput.SteerInput = LatestSteerInput;

	if (SurfaceSensor)
	{
		OutInput.SurfaceRoughness = SurfaceSensor->GetAverageRoughness();
		// GetAverageRoughness already returns 0 when fully airborne, but the
		// airborne flag is passed separately so a mode can suppress rumble
		// without inferring it from a zero.
		OutInput.bAirborne = FMath::IsNearlyZero(SurfaceSensor->GetAverageRoughness())
			&& SurfaceSensor->GetOffTrackWheelCount() == 0;
	}

	// Photo mode and the accessibility slider both act here, at the source,
	// rather than inside the mode — so a mode cannot forget to honour them.
	OutInput.ImpactImpulse = bPhotoMode ? 0.f : (PendingImpactImpulse * ShakeScale);
}

void UMidanChaseCameraComponent::ApplySpeedPostProcess(const float NormalisedSpeed)
{
	if (!Feel)
	{
		return;
	}

	// ART_DIRECTION §6: these are all speed-scaled rather than fixed. At
	// perceptible fixed strength they read as amateur; barely perceptible and
	// scaled by state, they read as photographic. If a reviewer can name the
	// effect, it is too strong.

	const float CA = MidanCurve::EvalSafe(Feel->ChromaticAberrationBySpeed, NormalisedSpeed, 0.f);
	if (CA > KINDA_SMALL_NUMBER)
	{
		PostProcessSettings.bOverride_SceneFringeIntensity = true;
		PostProcessSettings.SceneFringeIntensity = CA;
	}

	const float Vignette = MidanCurve::EvalSafe(Feel->VignetteBySpeed, NormalisedSpeed, 0.f);
	if (Vignette > KINDA_SMALL_NUMBER)
	{
		PostProcessSettings.bOverride_VignetteIntensity = true;
		PostProcessSettings.VignetteIntensity = Vignette;
	}

	// Motion blur is a feel system as much as a rendering one. The player's
	// slider multiplies it; ART_DIRECTION §7.3 forbids the BUDGET from cutting
	// it, which is a different thing from the player choosing to.
	const float Blur = MidanCurve::EvalSafe(Feel->MotionBlurBySpeed, NormalisedSpeed, 0.f);
	PostProcessSettings.bOverride_MotionBlurAmount = true;
	PostProcessSettings.MotionBlurAmount = bPhotoMode ? 0.f : (Blur * MotionBlurScale);
}

void UMidanChaseCameraComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInitialised || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FMidanCameraViewInput Input;
	BuildViewInput(Input, DeltaTime);

	// Consumed once. Leaving it set would make a single impact shake forever.
	PendingImpactImpulse = 0.f;

	GetActiveMode().UpdateView(Input, GetActiveConfig(), *Feel, DeltaTime, ViewState);

	// The modes work entirely in world space and know nothing about attachment.
	// Applying the result here rather than inside the mode is what keeps them
	// free of component concerns, and therefore unit-testable without a world.
	SetWorldLocationAndRotation(ViewState.Location, ViewState.Rotation);
	SetFieldOfView(ViewState.FOVDegrees);

	ApplySpeedPostProcess(Input.NormalisedSpeed);
}
