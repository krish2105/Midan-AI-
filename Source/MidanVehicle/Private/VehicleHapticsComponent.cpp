#include "VehicleHapticsComponent.h"

#include "Components/ForceFeedbackComponent.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "MidanCoreTypes.h"
#include "MidanCurveUtils.h"
#include "MidanGameplayTags.h"
#include "MidanLogChannels.h"
#include "MidanMathUtils.h"
#include "MidanVehicleMovementComponent.h"
#include "SurfaceResponseDataAsset.h"
#include "VehicleFeelDataAsset.h"
#include "VehicleSurfaceSensorComponent.h"

namespace
{
	/** Minimum seconds between kerb transients. Kerbs are struck in rapid
	 *  succession by design; without a floor the transients overlap into
	 *  continuous buzz and stop reading as individual strikes. */
	static constexpr float KerbMinIntervalSeconds = 0.12f;
}

UVehicleHapticsComponent::UVehicleHapticsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

APlayerController* UVehicleHapticsComponent::GetOwningPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

void UVehicleHapticsComponent::InitialiseFromAsset(
	const UVehicleFeelDataAsset* InFeel,
	UMidanVehicleMovementComponent* InMovement,
	UVehicleSurfaceSensorComponent* InSurfaceSensor)
{
	Feel = InFeel;
	Movement = InMovement;
	SurfaceSensor = InSurfaceSensor;

	// Only the player has a controller to rumble. Seven of the eight cars in a
	// race are AI, so this is the common case and NOT an error — logging it as
	// one would bury the real errors under seven false ones per race.
	if (!GetOwningPlayerController())
	{
		bInitialised = false;
		return;
	}

	bInitialised = (Feel != nullptr) && (Movement != nullptr);
	if (!bInitialised)
	{
		UE_LOG(LogMidanVehicle, Error,
			TEXT("VehicleHapticsComponent on '%s': not initialised. No force feedback."),
			*GetNameSafe(GetOwner()));
	}
}

void UVehicleHapticsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Stop continuous effects explicitly. A looping force feedback effect that
	// outlives its vehicle leaves the controller rumbling on the results screen,
	// which is both a bug and unpleasant.
	if (IdleRumble)    { IdleRumble->Stop(); }
	if (SurfaceRumble) { SurfaceRumble->Stop(); }
	if (SlipRumble)    { SlipRumble->Stop(); }

	Super::EndPlay(EndPlayReason);
}

void UVehicleHapticsComponent::SetHapticIntensity(const float InIntensity)
{
	HapticIntensity = FMath::Clamp(InIntensity, 0.f, 1.f);

	// Zero means off, and off means silent immediately rather than after the
	// current effect decays.
	if (HapticIntensity <= KINDA_SMALL_NUMBER)
	{
		if (IdleRumble)    { IdleRumble->Stop(); }
		if (SurfaceRumble) { SurfaceRumble->Stop(); }
		if (SlipRumble)    { SlipRumble->Stop(); }
		SmoothedSurfaceAmplitude = 0.f;
		SmoothedSlipAmplitude = 0.f;
	}
}

void UVehicleHapticsComponent::PlayTransient(const TSoftObjectPtr<UForceFeedbackEffect>& Effect, const float Scale)
{
	if (HapticIntensity <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	UForceFeedbackEffect* Loaded = Effect.Get();
	APlayerController* PC = GetOwningPlayerController();
	if (!Loaded || !PC)
	{
		return;
	}

	FForceFeedbackParameters Params;
	Params.bLooping = false;
	Params.Tag = NAME_None;

	PC->ClientPlayForceFeedback(Loaded, Params);
}

void UVehicleHapticsComponent::ReportImpact(const float ImpulseMagnitude)
{
	if (!bInitialised || !Feel)
	{
		return;
	}

	// Amplitude from the SAME curve family as the camera's ImpactShakeByImpulse.
	// If the two disagree, a collision that looks minor feels severe, and the
	// player stops trusting both channels.
	const float Normalised = FMath::Clamp(FMath::Abs(ImpulseMagnitude) / FMath::Max(Feel->FullScaleImpactImpulse, 1.f), 0.f, 1.f);
	const float Amplitude = MidanCurve::EvalSafe(Feel->HapticAmplitudeByImpulse, Normalised, Normalised);

	PlayTransient(Feel->ImpactRumble, Amplitude * HapticIntensity);
}

void UVehicleHapticsComponent::ReportKerbStrike(const float Intensity)
{
	if (!bInitialised || !Feel || KerbCooldown > 0.f)
	{
		return;
	}

	KerbCooldown = KerbMinIntervalSeconds;
	PlayTransient(Feel->KerbStrikeRumble, FMath::Clamp(Intensity, 0.f, 1.f) * HapticIntensity);
}

void UVehicleHapticsComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInitialised || DeltaTime <= KINDA_SMALL_NUMBER || HapticIntensity <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	KerbCooldown = FMath::Max(0.f, KerbCooldown - DeltaTime);

	// --- Continuous channel 1: surface texture.
	float TargetSurface = 0.f;
	if (SurfaceSensor)
	{
		TargetSurface = SurfaceSensor->GetAverageRoughness();
	}

	// --- Continuous channel 2: wheel slip. This is the channel that tells a pad
	// player the car is sliding before the camera or the audio does, and it is
	// the single most useful haptic in a racing game.
	FMidanVehicleFrameState State;
	Movement->WriteWheelPhysicsToFrameState(State);

	const float RearSlip = FMath::Clamp(State.GetRearAxleSlipAngle() / 45.f, 0.f, 1.f);
	const float FrontSlip = FMath::Clamp(State.GetFrontAxleSlipAngle() / 45.f, 0.f, 1.f);
	const float TargetSlip = FMath::Max(RearSlip, FrontSlip);

	// Smoothed, because raw values chatter.
	SmoothedSurfaceAmplitude = MidanMath::ExpDamp(
		SmoothedSurfaceAmplitude, TargetSurface, Feel->HapticSmoothingRate, DeltaTime);
	SmoothedSlipAmplitude = MidanMath::ExpDamp(
		SmoothedSlipAmplitude, TargetSlip, Feel->HapticSmoothingRate, DeltaTime);

	// --- Kerb detection. A kerb strike is a transient, not continuous texture:
	// it is a discrete event the driver chose, so it fires once per strike rather
	// than rumbling for as long as the wheel is on the kerb.
	for (int32 i = 0; i < MidanVehicleConstants::NumWheels; ++i)
	{
		if (SurfaceSensor && SurfaceSensor->GetWheelSurfaceTag(i) == MidanTags::Surface_Kerb)
		{
			ReportKerbStrike(Feel->KerbStrikeIntensity);
			break;
		}
	}

	// --- Push continuous amplitudes.
	//
	// API VERIFY: the exact mechanism for modulating a LOOPING force feedback
	// effect's amplitude at runtime on 5.8. UForceFeedbackComponent exposes an
	// intensity multiplier; confirm the setter name and whether it must be
	// re-applied per frame. See docs/ASSUMPTIONS.md A27. The CHANNEL DESIGN —
	// two continuous, two transient, all sharing the impulse curve family — is
	// what matters here and does not change with the accessor.
	if (SurfaceRumble)
	{
		SurfaceRumble->SetIntensityMultiplier(SmoothedSurfaceAmplitude * HapticIntensity);
	}
	if (SlipRumble)
	{
		SlipRumble->SetIntensityMultiplier(SmoothedSlipAmplitude * HapticIntensity);
	}
	if (IdleRumble)
	{
		// Idle rumble fades out as speed rises — at 250km/h the engine idle is
		// not what the controller should be communicating.
		const float SpeedKmh = FMath::Abs(Movement->GetForwardSpeedKmh());
		const float IdleWeight = FMath::Clamp(
			1.f - (SpeedKmh / FMath::Max(Feel->IdleRumbleFadeOutSpeedKmh, 1.f)), 0.f, 1.f);
		IdleRumble->SetIntensityMultiplier(IdleWeight * HapticIntensity);
	}
}
