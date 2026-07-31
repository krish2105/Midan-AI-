// Persisted player preferences: HUD scale, minimal-HUD, photo mode, and the
// engine-standard resolution/quality settings UGameUserSettings already owns.
//
// Responsibility: what the player has chosen, independent of any race.
// Single reason to change: a new player-facing preference is added.
//
// Distinct from UMidanSaveGame: settings are how the player wants the game
// to behave, records are what the player has achieved. Conflating them would
// mean a settings reset wipes best times, or a record wipe resets the HUD
// scale — two independent things a player has no reason to expect are linked.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "MidanGameUserSettings.generated.h"

UCLASS()
class MIDANCORE_API UMidanGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UMidanGameUserSettings();

	static UMidanGameUserSettings* Get();

	// --- HUD (ART_DIRECTION §8.4) ------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	float HUDScale = 1.f;

	/** Fades non-critical elements (assist flags, minimap, sector delta) —
	 *  ART_DIRECTION §8.5. Never hides the RPM/speed/gear/position cluster,
	 *  which stays essential regardless. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	bool bMinimalHUD = false;

	/** Hides the HUD entirely. Distinct from bMinimalHUD, which still shows
	 *  the essential cluster. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	bool bPhotoMode = false;

	// --- Audio ---------------------------------------------------------
	// Master and category volumes. Not vehicle tuning (which lives in
	// UVehicleFeelDataAsset) — this is player preference on top of it.

	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MasterVolume = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EngineVolume = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TyreVolume = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WindVolume = 1.f;

	/** Clamps every field into its legal range. Called from
	 *  UMidanSettingsWidget before ApplySettings — the same "clamp at the
	 *  door" discipline FMidanVehicleInputState::Sanitise uses. */
	UFUNCTION(BlueprintCallable, Category = "Midan|Settings")
	void SanitiseMidanSettings();

	//~ UGameUserSettings
	virtual void SetToDefaults() override;
};
