#include "VehicleAudioComponent.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MidanCoreTypes.h"
#include "MidanCurveUtils.h"
#include "MidanLogChannels.h"
#include "MidanMathUtils.h"
#include "MidanVehicleMovementComponent.h"
#include "Sound/SoundBase.h"
#include "VehicleFeelDataAsset.h"
#include "VehicleSurfaceSensorComponent.h"

namespace
{
	/** Seconds the Shifting flag stays true after a gear change. Long enough for
	 *  the graph to react, short enough not to overlap the next shift. Not a feel
	 *  dial — the audible shift character is authored in the MetaSound. */
	static constexpr float ShiftFlagDurationSeconds = 0.18f;

	/** Floor on impact volume, so even a light scrape is audible rather than
	 *  fading to nothing. Not a feel dial — a zero-volume impact is a missing
	 *  sound, which reads as a bug. */
	static constexpr float MinImpactVolume = 0.15f;

	/** Minimum seconds between backfires. */
	static constexpr float BackfireMinIntervalSeconds = 0.35f;

}

UVehicleAudioComponent::UVehicleAudioComponent()
{
	// Per-frame parameter push. Justified in docs/ARCHITECTURE.md §2.3 — audio
	// parameters are a presentation concern and frame rate is the right cadence.
	PrimaryComponentTick.bCanEverTick = true;
}

void UVehicleAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	PreviousGear = 0;
}

UAudioComponent* UVehicleAudioComponent::SpawnLoop(const TSoftObjectPtr<USoundBase>& Sound, const FName DebugName) const
{
	USoundBase* Loaded = Sound.Get();
	if (!Loaded)
	{
		// Not an error. A vehicle without a wind loop is legitimate during
		// Phase 4 bring-up, and the missing asset is already a validation
		// warning on the feel asset.
		UE_LOG(LogMidanVehicle, Verbose,
			TEXT("VehicleAudioComponent on '%s': no sound assigned for '%s'."),
			*GetNameSafe(GetOwner()), *DebugName.ToString());
		return nullptr;
	}

	UAudioComponent* Comp = UGameplayStatics::SpawnSoundAttached(
		Loaded,
		const_cast<UVehicleAudioComponent*>(this),
		NAME_None,
		FVector::ZeroVector,
		EAttachLocation::KeepRelativeOffset,
		/*bStopWhenAttachedToDestroyed*/ true,
		/*VolumeMultiplier*/ 1.f,
		/*PitchMultiplier*/ 1.f,
		/*StartTime*/ 0.f,
		/*ConcurrencySettings*/ nullptr,
		/*AttenuationSettings*/ nullptr,
		/*bAutoDestroy*/ false);

	return Comp;
}

void UVehicleAudioComponent::InitialiseFromAsset(
	const UVehicleFeelDataAsset* InFeel,
	UMidanVehicleMovementComponent* InMovement,
	UVehicleSurfaceSensorComponent* InSurfaceSensor,
	const float InMaxRPM)
{
	Feel = InFeel;
	Movement = InMovement;
	SurfaceSensor = InSurfaceSensor;
	MaxRPM = FMath::Max(InMaxRPM, 1.f);

	bInitialised = (Feel != nullptr) && (Movement != nullptr);
	if (!bInitialised)
	{
		UE_LOG(LogMidanVehicle, Error,
			TEXT("VehicleAudioComponent on '%s': not initialised. The vehicle will be silent."),
			*GetNameSafe(GetOwner()));
		return;
	}

	// Created once and kept. Spawning per frame would be an allocation and an
	// audible restart every frame.
	EngineLoop = SpawnLoop(Feel->EngineSound, TEXT("EngineSound"));
	TyreLoop = SpawnLoop(Feel->TyreScrubSound, TEXT("TyreScrubSound"));
	WindLoop = SpawnLoop(Feel->WindSound, TEXT("WindSound"));
}

void UVehicleAudioComponent::SetVehicleAudioScale(const float InScale)
{
	AudioScale = FMath::Clamp(InScale, 0.f, 1.f);

	// Applied as a volume multiplier on the loops rather than baked into the
	// parameters, so ducking does not disturb the blend positions.
	if (EngineLoop) { EngineLoop->SetVolumeMultiplier(AudioScale); }
	if (TyreLoop)   { TyreLoop->SetVolumeMultiplier(AudioScale); }
	if (WindLoop)   { WindLoop->SetVolumeMultiplier(AudioScale); }
}

float UVehicleAudioComponent::ComputeEngineLoad() const
{
	if (!Movement)
	{
		return 0.f;
	}

	const FMidanVehicleInputState& Input = Movement->GetEffectiveInput();
	const float Throttle = Input.Throttle;

	// Load is throttle weighted by whether the engine is meeting resistance.
	//
	// Not simply throttle: at full throttle with the driven wheels spinning, the
	// engine is meeting almost nothing, so load is LOW even though the pedal is
	// down. That is exactly what wheelspin sounds like, and a throttle-only
	// mapping loses it entirely.
	//
	// Resistance is approximated from mean driven-wheel grip: a wheel with high
	// slip is transmitting little.
	float MeanSlipMagnitude = 0.f;
	int32 Counted = 0;

	for (int32 i = 0; i < MidanVehicleConstants::NumWheels; ++i)
	{
		if (SurfaceSensor && !SurfaceSensor->IsWheelOffTrack(i))
		{
			MeanSlipMagnitude += FMath::Abs(Movement->GetWheelSlipRatio(i));
			++Counted;
		}
	}

	if (Counted > 0)
	{
		MeanSlipMagnitude /= static_cast<float>(Counted);
	}

	// Slip of 1.0 or more means the wheels are doing nothing useful.
	const float Traction = FMath::Clamp(1.f - MeanSlipMagnitude, 0.f, 1.f);

	// Engine braking counts as load too — a closed throttle at high RPM is
	// working against the drivetrain, and that is an audible state, not silence.
	const float NormalisedRPM = FMath::Clamp(Movement->GetEngineRotationSpeed() / MaxRPM, 0.f, 1.f);
	const float OverrunLoad = (1.f - Throttle) * NormalisedRPM * Feel->OverrunLoadWeight;

	return FMath::Clamp(Throttle * Traction + OverrunLoad, 0.f, 1.f);
}

void UVehicleAudioComponent::ReportImpact(const float ImpulseMagnitude)
{
	if (!Feel || !bInitialised)
	{
		return;
	}

	USoundBase* Impact = Feel->ImpactSound.Get();
	if (!Impact)
	{
		return;
	}

	// One-shot at the vehicle. Volume scaled by impulse so a scrape and a crash
	// are different events — the same principle as the camera's impact shake,
	// and driven from the same reported impulse so the two channels agree.
	const float Volume = FMath::Clamp(
		ImpulseMagnitude / FMath::Max(Feel->FullScaleImpactImpulse, 1.f), MinImpactVolume, 1.f) * AudioScale;

	// Small per-instance pitch variation. Fixed pitch on a repeated sound is
	// fatiguing within a minute.
	const float Pitch = FMath::FRandRange(0.94f, 1.06f);

	UGameplayStatics::SpawnSoundAttached(Impact, GetOwner()->GetRootComponent(), NAME_None,
		FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, Volume, Pitch);
}

void UVehicleAudioComponent::ReportBackfire()
{
	if (!Feel || !bInitialised || BackfireCooldown > 0.f)
	{
		return;
	}

	USoundBase* Backfire = Feel->BackfireSound.Get();
	if (!Backfire)
	{
		return;
	}

	BackfireCooldown = BackfireMinIntervalSeconds;

	UGameplayStatics::SpawnSoundAttached(Backfire, GetOwner()->GetRootComponent(), NAME_None,
		FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true,
		AudioScale, FMath::FRandRange(0.92f, 1.08f));
}

void UVehicleAudioComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInitialised || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	BackfireCooldown = FMath::Max(0.f, BackfireCooldown - DeltaTime);
	ShiftFlagTimer = FMath::Max(0.f, ShiftFlagTimer - DeltaTime);

	const float RPM = Movement->GetEngineRotationSpeed();
	const float NormalisedRPM = FMath::Clamp(RPM / MaxRPM, 0.f, 1.f);
	const float SpeedKmh = FMath::Abs(Movement->GetForwardSpeedKmh());
	const float NormaliseKmh = FMath::Max(Feel->SpeedNormalisationKmh, 1.f);
	const float NormalisedSpeed = FMath::Clamp(SpeedKmh / NormaliseKmh, 0.f, 1.f);
	const int32 Gear = Movement->GetCurrentGear();

	// --- Shift detection, for the graph's shift-aware behaviour and for the
	// backfire trigger.
	if (Gear != PreviousGear)
	{
		ShiftFlagTimer = ShiftFlagDurationSeconds;

		// Backfire on an UPSHIFT UNDER LOAD only. An upshift while coasting does
		// not backfire on a real car and sounds gratuitous on a fake one.
		if (Gear > PreviousGear && SmoothedLoad > Feel->BackfireLoadThreshold)
		{
			ReportBackfire();
		}
		PreviousGear = Gear;
	}

	// --- The second blend axis. Smoothed, because a raw crossfade chatters.
	const float RawLoad = ComputeEngineLoad();
	SmoothedLoad = MidanMath::ExpDamp(SmoothedLoad, RawLoad, Feel->EngineLoadSmoothingRate, DeltaTime);

	// --- Push the engine parameters. THIS is the two-axis blend: the MetaSound
	// graph holds four-plus RPM layers in separate on-load and off-load sets and
	// crossfades on both axes. Sound design stays in the graph.
	if (EngineLoop)
	{
		EngineLoop->SetFloatParameter(MidanAudioParams::RPM, NormalisedRPM);
		EngineLoop->SetFloatParameter(MidanAudioParams::Load, SmoothedLoad);
		EngineLoop->SetFloatParameter(MidanAudioParams::Speed, NormalisedSpeed);
		EngineLoop->SetIntParameter(MidanAudioParams::Gear, Gear);
		EngineLoop->SetBoolParameter(MidanAudioParams::Shifting, ShiftFlagTimer > 0.f);
	}

	// --- Tyre scrub, FRONT AND REAR SEPARATELY.
	//
	// A single combined slip value makes understeer and oversteer sound
	// identical, which throws away the clearest audio cue the player has about
	// which end of the car is letting go.
	if (TyreLoop)
	{
		FMidanVehicleFrameState State;
		Movement->WriteWheelPhysicsToFrameState(State);

		const float FrontSlip = FMath::Clamp(State.GetFrontAxleSlipAngle() / 45.f, 0.f, 1.f);
		const float RearSlip = FMath::Clamp(State.GetRearAxleSlipAngle() / 45.f, 0.f, 1.f);

		const float FrontGain = MidanCurve::EvalSafe(Feel->TyreScrubGainBySlip, FrontSlip, 0.f);
		const float RearGain = MidanCurve::EvalSafe(Feel->TyreScrubGainBySlip, RearSlip, 0.f);

		TyreLoop->SetFloatParameter(MidanAudioParams::SlipFront, FrontGain);
		TyreLoop->SetFloatParameter(MidanAudioParams::SlipRear, RearGain);

		if (SurfaceSensor)
		{
			// Surface changes the scrub character — gravel and tarmac are
			// different sounds, not the same sound at a different volume.
			TyreLoop->SetFloatParameter(MidanAudioParams::SurfaceRoughness, SurfaceSensor->GetAverageRoughness());
		}
	}

	// --- Wind. Speed only; it has no other meaningful axis.
	if (WindLoop)
	{
		const float WindGain = MidanCurve::EvalSafe(Feel->WindGainBySpeed, NormalisedSpeed, 0.f);
		WindLoop->SetFloatParameter(MidanAudioParams::Speed, NormalisedSpeed);
		WindLoop->SetVolumeMultiplier(WindGain * AudioScale);
	}
}
