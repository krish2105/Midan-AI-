// Every tunable HUD value: RPM colour thresholds, panel styling, safe margin,
// HUD scale bounds, and the colours ART_DIRECTION.md §8 specifies.
//
// Responsibility: the source of truth for HUD look and behaviour thresholds.
// Single reason to change: a new HUD value is introduced.
//
// CLAUDE.md forbids hardcoding tuning values, and a colour breakpoint (RPM
// strip goes amber at 80%, red at 92% — ART_DIRECTION §8.1) is exactly that:
// a value a designer adjusts by eye against the actual rendered HUD, not a
// constant a programmer picks once.

#pragma once

#include "CoreMinimal.h"
#include "MidanDataAsset.h"
#include "MidanHUDDataAsset.generated.h"

UCLASS(BlueprintType)
class MIDANRACE_API UMidanHUDDataAsset : public UMidanDataAsset
{
	GENERATED_BODY()

public:
	// --- RPM strip (ART_DIRECTION §8.1) -----------------------------------

	/** Normalised RPM (0..1 of MaxRPM) above which the strip goes amber. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RPM", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RPMAmberThreshold = 0.80f;

	/** Normalised RPM above which the strip goes red. Must exceed
	 *  RPMAmberThreshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RPM", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RPMRedThreshold = 0.92f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RPM")
	FLinearColor RPMNormalColor = FLinearColor(0.1f, 0.85f, 0.2f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RPM")
	FLinearColor RPMAmberColor = FLinearColor(1.f, 0.65f, 0.f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RPM")
	FLinearColor RPMRedColor = FLinearColor(0.9f, 0.1f, 0.1f, 1.f);

	// --- Panel styling (ART_DIRECTION §8.3) -------------------------------
	// "No backdrop blur" is not a field here — it is an absence, not a value,
	// enforced by never adding a blur material in the first place.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panels", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PanelOpacity = 0.72f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panels", meta = (ClampMin = "0.0"))
	float PanelCornerRadiusPx = 6.f;

	// --- Safe zone and scale (ART_DIRECTION §8.4) -------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0", ClampMax = "0.2"))
	float SafeMarginPercent = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float HUDScaleMin = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1.0", ClampMax = "2.0"))
	float HUDScaleMax = 1.5f;

	// --- Assist / off-track flags (ART_DIRECTION §8.5) --------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AssistFlags")
	FLinearColor AssistInterventionColor = FLinearColor(1.f, 0.65f, 0.f, 1.f); // amber

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AssistFlags")
	FLinearColor OffTrackColor = FLinearColor(0.9f, 0.1f, 0.1f, 1.f); // red

	// --- Sector delta (colourblind-safe: sign and arrow, never colour alone)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SectorDelta")
	FLinearColor SectorDeltaFasterColor = FLinearColor(0.1f, 0.85f, 0.2f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SectorDelta")
	FLinearColor SectorDeltaSlowerColor = FLinearColor(0.9f, 0.1f, 0.1f, 1.f);

	// --- Minimap (ART_DIRECTION §8.2: spline-generated, never a texture) --

	/** Points sampled evenly along the track spline to build the minimap
	 *  polyline. Higher is smoother through tight corners, at the cost of
	 *  one extra vector per sample in a cached array rebuilt only when the
	 *  track changes — never per frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap", meta = (ClampMin = "16", ClampMax = "512"))
	int32 MinimapSampleCount = 128;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap")
	FLinearColor MinimapTrackColor = FLinearColor(0.8f, 0.8f, 0.85f, 0.9f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap")
	FLinearColor MinimapPlayerDotColor = FLinearColor(1.f, 0.85f, 0.2f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap")
	FLinearColor MinimapOpponentDotColor = FLinearColor(0.7f, 0.7f, 0.75f, 0.85f);

	//~ UMidanDataAsset
	virtual void ValidateMidanData(FMidanValidationResult& Result) const override;
};
