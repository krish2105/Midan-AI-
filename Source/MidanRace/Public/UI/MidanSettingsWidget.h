// HUD scale, minimal-HUD, photo-mode, and volume sliders — the player-facing
// front end for UMidanGameUserSettings.
//
// Responsibility: read and write UMidanGameUserSettings, nothing else.
// Single reason to change: a new setting is exposed to the player.
//
// The HUD scale slider's range comes from UMidanHUDDataAsset::HUDScaleMin/Max
// (ART_DIRECTION §8.4's 0.75x-1.5x), not a hardcoded range here — this widget
// reads the same Data Asset AMidanHUD does, so the slider bounds and the
// HUD's own behaviour can never disagree about what "1.5x" means.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MidanSettingsWidget.generated.h"

class UCheckBox;
class USlider;
class UMidanHUDDataAsset;

UCLASS()
class MIDANRACE_API UMidanSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Slider range source — see the class comment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|HUD")
	TSoftObjectPtr<UMidanHUDDataAsset> HUDData;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USlider> HUDScaleSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCheckBox> MinimalHUDCheckBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCheckBox> PhotoModeCheckBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USlider> MasterVolumeSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USlider> EngineVolumeSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USlider> TyreVolumeSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USlider> WindVolumeSlider;

	//~ UUserWidget
	virtual void NativeConstruct() override;

private:
	void OnHUDDataLoaded();

	/** Pushes UMidanGameUserSettings' current values into every bound
	 *  widget. Called once at construct, after HUDData resolves (for the
	 *  slider range) — not every tick, since nothing outside this widget
	 *  changes these settings while it is open. */
	void RefreshFromSettings();

	UFUNCTION()
	void HandleHUDScaleChanged(float NewValue);
	UFUNCTION()
	void HandleMinimalHUDChanged(bool bNewValue);
	UFUNCTION()
	void HandlePhotoModeChanged(bool bNewValue);
	UFUNCTION()
	void HandleMasterVolumeChanged(float NewValue);
	UFUNCTION()
	void HandleEngineVolumeChanged(float NewValue);
	UFUNCTION()
	void HandleTyreVolumeChanged(float NewValue);
	UFUNCTION()
	void HandleWindVolumeChanged(float NewValue);

	/** Sanitises, applies, and saves — called after every change. Settings
	 *  are cheap enough (a handful of floats/bools) that saving on every
	 *  slider tick rather than debouncing is simpler and the write cost is
	 *  negligible against a settings file this small. */
	void ApplyAndSave();

	UPROPERTY(Transient)
	TObjectPtr<const UMidanHUDDataAsset> LoadedHUDData;

	TSharedPtr<struct FStreamableHandle> HUDDataLoadHandle;
};
