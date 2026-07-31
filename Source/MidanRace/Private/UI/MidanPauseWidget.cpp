#include "UI/MidanPauseWidget.h"

#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/MidanSettingsWidget.h"

void UMidanPauseWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddDynamic(this, &UMidanPauseWidget::HandleResumeClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UMidanPauseWidget::HandleSettingsClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UMidanPauseWidget::HandleQuitClicked);
	}
	if (Settings)
	{
		Settings->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMidanPauseWidget::HandleResumeClicked()
{
	OnResumeRequested.ExecuteIfBound();
}

void UMidanPauseWidget::HandleSettingsClicked()
{
	if (Settings)
	{
		Settings->SetVisibility(ESlateVisibility::Visible);
	}
}

void UMidanPauseWidget::HandleQuitClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}
