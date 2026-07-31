// A minimap generated from the track spline, never an authored texture.
//
// Responsibility: draw the circuit outline and every racer's live position.
// Single reason to change: how the minimap is drawn or what it shows changes.
//
// ART_DIRECTION §8.2: "the minimap must be spline-generated... draw it from
// AMidanTrackSpline sampled at fixed arc length, so it stays correct when you
// change the track. An authored minimap texture goes stale the first time
// you move a corner." The polyline is cached once (NativeConstruct, rebuilt
// only if the track changes) and drawn with FSlateDrawElement lines in
// NativePaint — no render target, no per-frame allocation, just a cached
// point array and a paint call.
//
// Reads racer positions directly from AMidanRaceGameState::PlayerArray —
// this widget lives in MidanRace, so referencing MidanRace's own GameState
// is not a cross-module concern the way it would be from MidanAI or
// MidanVehicle.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MidanMinimapWidget.generated.h"

class UMidanHUDDataAsset;

UCLASS()
class MIDANRACE_API UMidanMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Style and sample-count source. Soft, async-loaded like every other
	 *  Data Asset reference in this project. Set on the WBP_ Blueprint
	 *  subclass — see docs/MANUAL_STEPS.md Phase 7. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|HUD")
	TSoftObjectPtr<UMidanHUDDataAsset> HUDData;

	/** Rebuilds the cached polyline from the currently registered track.
	 *  Called once from NativeConstruct; exposed so a future track-change
	 *  event (not needed for a single-circuit vertical slice) can call it
	 *  again without a widget recreate. */
	UFUNCTION(BlueprintCallable, Category = "Midan|HUD")
	void RebuildTrackPolyline();

	//~ UUserWidget
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	void OnHUDDataLoaded();

	/** Cached, normalised to a [0,1] square with aspect preserved (both axes
	 *  share one scale factor, so the track's true shape is not stretched
	 *  to fill a non-square widget). */
	UPROPERTY(Transient)
	TArray<FVector2D> NormalisedTrackPoints;

	UPROPERTY(Transient)
	TObjectPtr<const UMidanHUDDataAsset> LoadedHUDData;

	TSharedPtr<struct FStreamableHandle> HUDDataLoadHandle;
};
