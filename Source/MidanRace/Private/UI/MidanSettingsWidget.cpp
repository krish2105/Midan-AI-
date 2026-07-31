#include "UI/MidanSettingsWidget.h"

#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Engine/AssetManager.h"
#include "MidanGameUserSettings.h"
#include "UI/MidanHUDDataAsset.h"

void UMidanSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (HUDScaleSlider)
	{
		HUDScaleSlider->OnValueChanged.AddDynamic(this, &UMidanSettingsWidget::HandleHUDScaleChanged);
	}
	if (MinimalHUDCheckBox)
	{
		MinimalHUDCheckBox->OnCheckStateChanged.AddDynamic(this, &UMidanSettingsWidget::HandleMinimalHUDChanged);
	}
	if (PhotoModeCheckBox)
	{
		PhotoModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UMidanSettingsWidget::HandlePhotoModeChanged);
	}
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->OnValueChanged.AddDynamic(this, &UMidanSettingsWidget::HandleMasterVolumeChanged);
	}
	if (EngineVolumeSlider)
	{
		EngineVolumeSlider->OnValueChanged.AddDynamic(this, &UMidanSettingsWidget::HandleEngineVolumeChanged);
	}
	if (TyreVolumeSlider)
	{
		TyreVolumeSlider->OnValueChanged.AddDynamic(this, &UMidanSettingsWidget::HandleTyreVolumeChanged);
	}
	if (WindVolumeSlider)
	{
		WindVolumeSlider->OnValueChanged.AddDynamic(this, &UMidanSettingsWidget::HandleWindVolumeChanged);
	}

	if (!HUDData.IsNull())
	{
		HUDDataLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			HUDData.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &UMidanSettingsWidget::OnHUDDataLoaded));
	}

	RefreshFromSettings();
}

void UMidanSettingsWidget::OnHUDDataLoaded()
{
	LoadedHUDData = HUDData.Get();
	RefreshFromSettings();
}

void UMidanSettingsWidget::RefreshFromSettings()
{
	UMidanGameUserSettings* Settings = UMidanGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}

	if (HUDScaleSlider)
	{
		const float Min = LoadedHUDData ? LoadedHUDData->HUDScaleMin : 0.75f;
		const float Max = LoadedHUDData ? LoadedHUDData->HUDScaleMax : 1.5f;
		HUDScaleSlider->SetMinValue(Min);
		HUDScaleSlider->SetMaxValue(Max);
		HUDScaleSlider->SetValue(FMath::Clamp(Settings->HUDScale, Min, Max));
	}
	if (MinimalHUDCheckBox)
	{
		MinimalHUDCheckBox->SetIsChecked(Settings->bMinimalHUD);
	}
	if (PhotoModeCheckBox)
	{
		PhotoModeCheckBox->SetIsChecked(Settings->bPhotoMode);
	}
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->SetValue(Settings->MasterVolume);
	}
	if (EngineVolumeSlider)
	{
		EngineVolumeSlider->SetValue(Settings->EngineVolume);
	}
	if (TyreVolumeSlider)
	{
		TyreVolumeSlider->SetValue(Settings->TyreVolume);
	}
	if (WindVolumeSlider)
	{
		WindVolumeSlider->SetValue(Settings->WindVolume);
	}
}

void UMidanSettingsWidget::HandleHUDScaleChanged(float NewValue)
{
	if (UMidanGameUserSettings* Settings = UMidanGameUserSettings::Get())
	{
		Settings->HUDScale = NewValue;
		ApplyAndSave();
	}
}

void UMidanSettingsWidget::HandleMinimalHUDChanged(bool bNewValue)
{
	if (UMidanGameUserSettings* Settings = UMidanGameUserSettings::Get())
	{
		Settings->bMinimalHUD = bNewValue;
		ApplyAndSave();
	}
}

void UMidanSettingsWidget::HandlePhotoModeChanged(bool bNewValue)
{
	if (UMidanGameUserSettings* Settings = UMidanGameUserSettings::Get())
	{
		Settings->bPhotoMode = bNewValue;
		ApplyAndSave();
	}
}

void UMidanSettingsWidget::HandleMasterVolumeChanged(float NewValue)
{
	if (UMidanGameUserSettings* Settings = UMidanGameUserSettings::Get())
	{
		Settings->MasterVolume = NewValue;
		ApplyAndSave();
	}
}

void UMidanSettingsWidget::HandleEngineVolumeChanged(float NewValue)
{
	if (UMidanGameUserSettings* Settings = UMidanGameUserSettings::Get())
	{
		Settings->EngineVolume = NewValue;
		ApplyAndSave();
	}
}

void UMidanSettingsWidget::HandleTyreVolumeChanged(float NewValue)
{
	if (UMidanGameUserSettings* Settings = UMidanGameUserSettings::Get())
	{
		Settings->TyreVolume = NewValue;
		ApplyAndSave();
	}
}

void UMidanSettingsWidget::HandleWindVolumeChanged(float NewValue)
{
	if (UMidanGameUserSettings* Settings = UMidanGameUserSettings::Get())
	{
		Settings->WindVolume = NewValue;
		ApplyAndSave();
	}
}

void UMidanSettingsWidget::ApplyAndSave()
{
	UMidanGameUserSettings* Settings = UMidanGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}
	Settings->SanitiseMidanSettings();
	Settings->ApplySettings(false);
	Settings->SaveSettings();
}
