#include "MidanGameUserSettings.h"

namespace
{
	// A wide structural sanity bound, NOT the player-facing slider range.
	// ART_DIRECTION §8.4's 0.75x-1.5x range is enforced by
	// UMidanSettingsWidget against UMidanHUDDataAsset::HUDScaleMin/Max —
	// this class cannot reference that Data Asset (it lives in MidanRace,
	// and MidanCore depends on nothing project-side). This clamp exists only
	// to stop a corrupted config value from producing an unusable HUD.
	constexpr float HUDScaleSanityMin = 0.25f;
	constexpr float HUDScaleSanityMax = 3.f;
}

UMidanGameUserSettings::UMidanGameUserSettings()
{
	SetToDefaults();
}

UMidanGameUserSettings* UMidanGameUserSettings::Get()
{
	return Cast<UMidanGameUserSettings>(GEngine->GetGameUserSettings());
}

void UMidanGameUserSettings::SanitiseMidanSettings()
{
	HUDScale = FMath::Clamp(HUDScale, HUDScaleSanityMin, HUDScaleSanityMax);
	MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
	EngineVolume = FMath::Clamp(EngineVolume, 0.f, 1.f);
	TyreVolume = FMath::Clamp(TyreVolume, 0.f, 1.f);
	WindVolume = FMath::Clamp(WindVolume, 0.f, 1.f);
}

void UMidanGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	HUDScale = 1.f;
	bMinimalHUD = false;
	bPhotoMode = false;
	MasterVolume = 1.f;
	EngineVolume = 1.f;
	TyreVolume = 1.f;
	WindVolume = 1.f;
}
