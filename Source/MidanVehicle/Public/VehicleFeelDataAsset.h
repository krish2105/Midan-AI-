// Camera, audio, FX and haptics for one vehicle. Roughly 40% of how the car is
// perceived, and about 10% of the code.
//
// Responsibility: hold every feel tuning value for one vehicle.
// Single reason to change: a new feel channel is introduced.
//
// EVERY VALUE HERE IS A CURVE, NOT A CONSTANT. A constant is right at one speed
// and wrong everywhere else. Camera defaults come from docs/ART_DIRECTION.md §5,
// which overrides master prompt §2.1 where they disagree (see ASSUMPTIONS A11).
//
// Phase 2 defines this schema. Phase 4 implements the components that consume
// it and prints the full list of curves a human must author.

#pragma once

#include "CoreMinimal.h"
#include "MidanDataAsset.h"
#include "Curves/CurveFloat.h"
#include "VehicleFeelDataAsset.generated.h"

class UForceFeedbackEffect;
class UNiagaraSystem;
class UMaterialInterface;
class USoundBase;

/** One chase-camera configuration. Four of these give the four modes. */
USTRUCT(BlueprintType)
struct MIDANVEHICLE_API FMidanCameraModeConfig
{
	GENERATED_BODY()

	/** ART_DIRECTION §5: the reference is a fairly tight lens at 72°. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FOV", meta = (ClampMin = "40.0", ClampMax = "140.0"))
	float BaseFOVDegrees = 72.f;

	/**
	 * FOV added, against normalised speed 0..1.
	 *
	 * THE SINGLE BIGGEST SPEED-SENSATION LEVER. ART_DIRECTION §5 wants 72° at
	 * rest reaching 95-100° at top speed, so this curve ends around 23-28.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FOV")
	FRuntimeFloatCurve FOVOffsetBySpeed;

	/** Spring arm length in cm. ART_DIRECTION §5: 5.5-6.5m, so 550-650. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0"))
	float ArmLengthCm = 600.f;

	/** Height above the vehicle origin, cm. ART_DIRECTION §5: 1.8-2.2m. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement")
	float HeightCm = 200.f;

	/** Downward pitch, degrees, negative looks down. ART_DIRECTION §5: -6 to -9,
	 *  which keeps the horizon in the upper third. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
	float PitchDegrees = -7.f;

	/** Location lag rate against normalised speed. ART_DIRECTION §5: 8-12.
	 *  Lag is what makes the car feel like it has mass the camera chases. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lag")
	FRuntimeFloatCurve LocationLagBySpeed;

	/** Rotation lag rate against normalised speed. ART_DIRECTION §5: 6-9. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lag")
	FRuntimeFloatCurve RotationLagBySpeed;

	/** Yaw offset from steering input, degrees at full lock. ART_DIRECTION §5:
	 *  ±4°. The camera anticipates the corner rather than following it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float LookAheadYawDegrees = 4.f;

	/** Yaw from chassis slip angle, degrees at full slip. ART_DIRECTION §5: ±6°.
	 *  Without this a drift looks like a bug rather than a slide. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float SlipYawDegrees = 6.f;

	/** Vertical suspension chatter suppression, 0 none to 1 full. Large impacts
	 *  still come through; only high-frequency motion is filtered, because
	 *  suspension chatter reaching the camera is nauseating. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VerticalDamping = 0.7f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour")
	bool bUseCollisionTest = true;

	/**
	 * How fast look-ahead yaw converges on its target, 1/seconds.
	 *
	 * Added at Phase 4. Higher makes the camera lead the steering more eagerly;
	 * too high and it snaps, which reads as the camera twitching rather than
	 * anticipating. This is a feel value a human tunes, so it is here rather
	 * than a constant in the camera code.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lag", meta = (ClampMin = "0.5", UIMax = "20.0"))
	float LookAheadDampingRate = 5.f;

	/**
	 * How fast slip yaw converges, 1/seconds.
	 *
	 * Deliberately faster than look-ahead by default: a slide begins abruptly
	 * and the camera must keep up or the drift is over before it reads.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lag", meta = (ClampMin = "0.5", UIMax = "20.0"))
	float SlipYawDampingRate = 7.f;

	/** Vertical damping rate when VerticalDamping is 0, 1/seconds. Together with
	 *  the min, defines what the 0..1 dial means. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "1.0", UIMax = "200.0"))
	float VerticalDampingRateMax = 60.f;

	/** Vertical damping rate when VerticalDamping is 1, 1/seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.5", UIMax = "60.0"))
	float VerticalDampingRateMin = 4.f;
};

UCLASS(BlueprintType)
class MIDANVEHICLE_API UVehicleFeelDataAsset : public UMidanDataAsset
{
	GENERATED_BODY()

public:
	// --- Camera --------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	FMidanCameraModeConfig ChaseFar;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	FMidanCameraModeConfig ChaseNear;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	FMidanCameraModeConfig Bonnet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	FMidanCameraModeConfig Cockpit;

	/** Speed treated as 1.0 for every speed-normalised curve above, km/h.
	 *  Shared so all curves use the same normalisation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "50.0"))
	float SpeedNormalisationKmh = 320.f;

	/** Camera shake amplitude against impact impulse magnitude. Unscaled shake
	 *  makes every impact feel identical, so none of them feel big. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Shake")
	FRuntimeFloatCurve ImpactShakeByImpulse;

	/** Rumble amplitude against surface roughness 0..1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Shake")
	FRuntimeFloatCurve SurfaceRumbleByRoughness;

	/**
	 * How fast impact shake decays, 1/seconds. Added at Phase 4.
	 *
	 * Fast enough that a shake reads as an impact rather than a wobble. Too slow
	 * and every kerb strike leaves the camera swimming for a second afterwards.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Shake", meta = (ClampMin = "0.5", UIMax = "30.0"))
	float ShakeDecayRate = 6.f;

	/** How fast surface rumble fades in and out when crossing a surface
	 *  boundary, 1/seconds. Damped so entering gravel fades rather than jolts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Shake", meta = (ClampMin = "0.5", UIMax = "30.0"))
	float RumbleFadeRate = 8.f;

	/**
	 * Impulse treated as a full-scale impact, Newton-seconds.
	 *
	 * The denominator that normalises a collision before it reaches
	 * ImpactShakeByImpulse and HapticAmplitudeByImpulse. Shared by the camera,
	 * audio and haptic channels so all three agree what "big" means — three
	 * channels disagreeing about the same collision is worse than one missing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Shake", meta = (ClampMin = "100.0", UIMax = "50000.0"))
	float FullScaleImpactImpulse = 5000.f;

	// --- Audio ---------------------------------------------------------------

	/**
	 * Engine MetaSound. Parameter mapping is C++; sound design stays in the graph.
	 *
	 * Phase 4 pushes a TWO-AXIS blend: RPM on one axis, engine load on the
	 * other, with separate on-load and off-load sample sets. Single-axis RPM
	 * blending produces a whine; two axes produce an engine.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> EngineSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> TyreScrubSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> WindSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> BackfireSound;

	/** Wind gain against normalised speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	FRuntimeFloatCurve WindGainBySpeed;

	/** Tyre scrub gain against slip magnitude 0..1. Per-axle at runtime, so
	 *  oversteer sounds different from understeer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	FRuntimeFloatCurve TyreScrubGainBySlip;

	//~ Audio feel values, added at Phase 4. Each affects how the engine SOUNDS
	//  under a given physical state, which is exactly the kind of thing a human
	//  tunes by ear — so none of them is a constant in the audio code.

	/**
	 * How fast engine load converges, 1/seconds.
	 *
	 * Load is the second blend axis. Raw load is noisy at the substep level, and
	 * an unsmoothed crossfade between the on-load and off-load sample sets
	 * sounds like a fault rather than a transition. Too slow and the engine
	 * responds audibly late to the throttle.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Engine", meta = (ClampMin = "1.0", UIMax = "40.0"))
	float EngineLoadSmoothingRate = 9.f;

	/**
	 * How much closed-throttle overrun counts as load, 0..1.
	 *
	 * A closed throttle at high RPM is working against the drivetrain, and that
	 * is an audible state rather than silence. Raise it for a car with strong
	 * engine braking; zero makes lifting off go quiet.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Engine", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OverrunLoadWeight = 0.35f;

	/** Engine load above which an upshift triggers a backfire, 0..1. An upshift
	 *  while coasting does not backfire on a real car and sounds gratuitous on a
	 *  fake one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Engine", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackfireLoadThreshold = 0.6f;

	// --- Visual FX -----------------------------------------------------------

	/** Tyre smoke spawn rate against slip 0..1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FX")
	FRuntimeFloatCurve TyreSmokeRateBySlip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FX")
	TSoftObjectPtr<UNiagaraSystem> TyreSmokeSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FX")
	TSoftObjectPtr<UNiagaraSystem> ExhaustBackfireSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FX")
	TSoftObjectPtr<UMaterialInterface> SkidDecalMaterial;

	/**
	 * Maximum simultaneous skid decals for this vehicle.
	 *
	 * POOLED, not unbounded. Decals deposited over a session are a slow leak
	 * that only shows up on lap four. Oldest is recycled.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FX", meta = (ClampMin = "0", ClampMax = "512"))
	int32 SkidDecalPoolSize = 96;

	/** Slip above which a decal is deposited. Gate, not a physics threshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SkidDecalSlipThreshold = 0.35f;

	// --- Post process --------------------------------------------------------

	/** Chromatic aberration against normalised speed. ART_DIRECTION §6: 0.2-0.4,
	 *  barely perceptible at rest, noticeable at speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post")
	FRuntimeFloatCurve ChromaticAberrationBySpeed;

	/** Radial vignette against normalised speed. ART_DIRECTION §6: 0.3-0.4. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post")
	FRuntimeFloatCurve VignetteBySpeed;

	/** Object motion blur against normalised speed. ART_DIRECTION §6: 0.4-0.5.
	 *  Player-facing intensity slider multiplies this; never cut it entirely,
	 *  it is feel rather than fidelity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post")
	FRuntimeFloatCurve MotionBlurBySpeed;

	// --- Haptics -------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Haptics")
	TSoftObjectPtr<UForceFeedbackEffect> EngineIdleRumble;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Haptics")
	TSoftObjectPtr<UForceFeedbackEffect> SurfaceRoughnessRumble;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Haptics")
	TSoftObjectPtr<UForceFeedbackEffect> WheelSlipRumble;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Haptics")
	TSoftObjectPtr<UForceFeedbackEffect> ImpactRumble;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Haptics")
	TSoftObjectPtr<UForceFeedbackEffect> KerbStrikeRumble;

	/** Haptic amplitude against impulse magnitude. Shares the curve family with
	 *  ImpactShakeByImpulse so visual, audio and haptic channels agree. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Haptics")
	FRuntimeFloatCurve HapticAmplitudeByImpulse;

	/** Transient intensity for a kerb strike, 0..1. Added at Phase 4. A kerb is
	 *  a deliberate part of driving, so it should feel like texture rather than
	 *  damage — noticeably below a collision. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float KerbStrikeIntensity = 0.6f;

	/** How fast continuous haptic amplitudes converge, 1/seconds. Raw values
	 *  chatter, and a chattering rumble reads as a faulty controller rather
	 *  than a rough surface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Haptics", meta = (ClampMin = "1.0", UIMax = "40.0"))
	float HapticSmoothingRate = 10.f;

	/** Speed above which engine idle rumble has fully faded out, km/h. At
	 *  250km/h the idle is not what the controller should be communicating. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Haptics", meta = (ClampMin = "5.0", UIMax = "200.0"))
	float IdleRumbleFadeOutSpeedKmh = 60.f;

	//~ Begin UMidanDataAsset interface
	virtual void ValidateMidanData(FMidanValidationResult& Result) const override;
	//~ End UMidanDataAsset interface

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
