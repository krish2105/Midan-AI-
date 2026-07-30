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

	//~ Begin UMidanDataAsset interface
	virtual void ValidateMidanData(FMidanValidationResult& Result) const override;
	//~ End UMidanDataAsset interface

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
