// The persistent race panel (RPM/speed/gear/position/sector/assist flags/
// minimap) and the AHUD that owns it plus the state-driven overlay widgets.
//
// Responsibility: assemble and own every HUD widget; the persistent panel's
// own live-data binding.
// Single reason to change: which widgets exist, or what the persistent panel
// displays, changes.
//
// Two classes in one file, a deliberate deviation from the file-level plan's
// one-class-per-header convention (docs/PHASE_PLAN.md records it): the plan
// lists only MidanHUD.h/.cpp for this responsibility, and the persistent
// panel (UMidanHUDWidget) has nowhere else to live without inventing a file
// the plan never named. AMidanHUD stays thin — create, own, show/hide — and
// UMidanHUDWidget holds the live-data binding, so the split in
// responsibility is still real even though the file split is not.
//
// ART_DIRECTION §8.1 layout: RPM strip adjacent to the gear readout (§8.2
// correction 1), minimap generated from the track spline (§8.2 correction 2,
// see UMidanMinimapWidget), assist/off-track flags top-left (§8.5), no
// backdrop blur anywhere (§8.3).
//
// UMG widget Tick is the standard mechanism for reading render-rate state —
// this is the same exemption UMidanChaseCameraComponent's per-frame Tick
// already established ("a render-rate concern by definition"), applied to
// UI. Added to the tick inventory in docs/ARCHITECTURE.md §2.3.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Blueprint/UserWidget.h"
#include "MidanHUD.generated.h"

class UTextBlock;
class UProgressBar;
class UMidanHUDDataAsset;
class UMidanMinimapWidget;
class UMidanCountdownWidget;
class UMidanResultsWidget;
class UMidanPauseWidget;
class UInputAction;

/** The always-on race panel: RPM, speed, gear, position, lap, sector delta,
 *  assist/off-track flags, and an embedded minimap. */
UCLASS()
class MIDANRACE_API UMidanHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|HUD")
	TSoftObjectPtr<UMidanHUDDataAsset> HUDData;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> SpeedText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> GearText;

	/** ART_DIRECTION §8.2 correction 1: adjacent to GearText in the WBP_
	 *  layout, not floating elsewhere — a layout instruction for
	 *  docs/MANUAL_STEPS.md, not something C++ enforces. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> RPMBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> PositionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> LapText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> SectorDeltaText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TCFlagText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ABSFlagText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> OffTrackFlagText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMidanMinimapWidget> Minimap;

	//~ UUserWidget
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void OnHUDDataLoaded();

	UPROPERTY(Transient)
	TObjectPtr<const UMidanHUDDataAsset> LoadedHUDData;

	TSharedPtr<struct FStreamableHandle> HUDDataLoadHandle;
};

UCLASS()
class MIDANRACE_API AMidanHUD : public AHUD
{
	GENERATED_BODY()

public:
	AMidanHUD();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|HUD")
	TSoftClassPtr<UMidanHUDWidget> MainHUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|HUD")
	TSoftClassPtr<UMidanCountdownWidget> CountdownWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|HUD")
	TSoftClassPtr<UMidanResultsWidget> ResultsWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|HUD")
	TSoftClassPtr<UMidanPauseWidget> PauseWidgetClass;

	/** IA_Pause, authored per docs/MANUAL_STEPS.md 1.3. Bound directly by
	 *  this actor via EnableInput — the same self-binding pattern
	 *  UMidanRespawnComponent uses for IA_Respawn, since neither has a
	 *  natural home inside UVehicleInputComponent (MidanVehicle does not
	 *  know MidanRace's UI exists). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|HUD")
	TSoftObjectPtr<UInputAction> PauseAction;

	UFUNCTION(BlueprintCallable, Category = "Midan|HUD")
	void TogglePause();

	//~ AHUD
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnWidgetClassesLoaded();
	void OnPauseActionLoaded();
	void HandleResumeRequested();

	TSharedPtr<struct FStreamableHandle> WidgetClassesLoadHandle;
	TSharedPtr<struct FStreamableHandle> PauseActionLoadHandle;

	UPROPERTY(Transient)
	TObjectPtr<UMidanHUDWidget> MainHUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UMidanCountdownWidget> CountdownWidget;

	UPROPERTY(Transient)
	TObjectPtr<UMidanResultsWidget> ResultsWidget;

	UPROPERTY(Transient)
	TObjectPtr<UMidanPauseWidget> PauseWidget;

	bool bIsPaused = false;
};
