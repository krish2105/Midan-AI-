// Resume / Settings / Quit. Shown and hidden by AMidanHUD, not self-managed —
// pause is a player-input event (IA_Pause), not a race-state transition.
//
// Responsibility: the pause menu's three actions.
// Single reason to change: a pause-menu action is added or removed.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MidanPauseWidget.generated.h"

class UButton;
class UMidanSettingsWidget;

UCLASS()
class MIDANRACE_API UMidanPauseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> QuitButton;

	/** Nested settings panel, shown in place of the three buttons when
	 *  SettingsButton is pressed. Authored as a child widget in the WBP_, not
	 *  a separately-added top-level widget — settings only make sense while
	 *  paused, in this vertical slice. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMidanSettingsWidget> Settings;

	/** Set by AMidanHUD, which owns the request to close (unpausing and
	 *  hiding this widget). Kept as a delegate rather than this widget
	 *  reaching back into AMidanHUD, so the widget has no upward dependency. */
	DECLARE_DELEGATE(FOnResumeRequested)
	FOnResumeRequested OnResumeRequested;

	//~ UUserWidget
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleQuitClicked();
};
