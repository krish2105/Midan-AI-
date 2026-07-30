// Surface tag -> friction, audio and FX response.
//
// Responsibility: map each physical surface to its handling and feedback response.
// Single reason to change: a new surface type is added to Config/DefaultEngine.ini.
//
// Per-wheel surface state is read from PHYSICAL MATERIALS, never trigger volumes.
// Trigger volumes duplicate the track shape, go stale the first time a corner
// moves, and cannot express "two wheels on gravel" — which is precisely the
// state that makes the rally car interesting.

#pragma once

#include "CoreMinimal.h"
#include "MidanDataAsset.h"
#include "GameplayTagContainer.h"
#include "Chaos/ChaosEngineInterface.h"
#include "SurfaceResponseDataAsset.generated.h"

class UNiagaraSystem;
class USoundBase;
class UMaterialInterface;
class UForceFeedbackEffect;

USTRUCT(BlueprintType)
struct MIDANVEHICLE_API FSurfaceResponseRow
{
	GENERATED_BODY()

	/** Must be a descendant of Surface. Mirrors an entry in DefaultEngine.ini. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (Categories = "Surface"))
	FGameplayTag SurfaceTag;

	/**
	 * The engine surface type this tag corresponds to.
	 *
	 * The DefaultEngine.ini PhysicalSurfaces list is append-only for this
	 * reason: reordering it silently remaps every physical material in Content,
	 * turning tarmac into gravel with no error anywhere.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	TEnumAsByte<EPhysicalSurface> PhysicalSurface = SurfaceType1;

	/**
	 * Grip multiplier. Tarmac is 1.0 by convention and everything else is
	 * relative to it.
	 *
	 * Handling consequence: this is the number that makes the rally car's
	 * "slow on tarmac, unstoppable on gravel" real. If gravel is only slightly
	 * below tarmac, surface choice stops mattering and one of the three cars
	 * loses its reason to exist.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Handling", meta = (ClampMin = "0.01", UIMax = "2.0"))
	float FrictionMultiplier = 1.f;

	/** Rolling resistance. Off-track surfaces should slow the car noticeably
	 *  even when grip alone would not. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Handling", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float RollingResistance = 0.01f;

	/** Surface roughness 0..1, driving camera rumble and haptics. Not physics. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Roughness = 0.f;

	/**
	 * Whether a wheel on this surface counts as on-track.
	 *
	 * Kerb and Wet are on-track; gravel, grass and sand are not. Read by
	 * UMidanLapTimingSubsystem at Phase 5 with a grace-time threshold, so
	 * clipping a kerb does not invalidate a lap.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	bool bCountsAsOnTrack = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FX")
	TSoftObjectPtr<UNiagaraSystem> WheelParticleSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FX")
	TSoftObjectPtr<UMaterialInterface> TyreMarkDecal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> RollingSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> SlipSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Haptics")
	TSoftObjectPtr<UForceFeedbackEffect> RumbleEffect;
};

UCLASS(BlueprintType)
class MIDANVEHICLE_API USurfaceResponseDataAsset : public UMidanDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surfaces", meta = (TitleProperty = "SurfaceTag"))
	TArray<FSurfaceResponseRow> Surfaces;

	/** Response used when a wheel reports a surface with no row. Returns null
	 *  only if the asset is empty, which validation errors on. */
	const FSurfaceResponseRow* FindRow(EPhysicalSurface InSurface) const;

	const FSurfaceResponseRow* FindRowByTag(const FGameplayTag& InTag) const;

	//~ Begin UMidanDataAsset interface
	virtual void ValidateMidanData(FMidanValidationResult& Result) const override;
	//~ End UMidanDataAsset interface

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
