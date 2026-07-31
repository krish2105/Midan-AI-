// The grid-start countdown: 3, 2, 1, GO.
//
// Responsibility: show the countdown and nothing else, only while it is
// running.
// Single reason to change: how the countdown is displayed changes.
//
// Self-managing visibility: reads IMidanRaceStateInterface every tick and
// shows/hides itself, rather than AMidanHUD polling race state centrally and
// pushing visibility to every widget. Each state-driven widget in Source/
// MidanRace/UI owns its own visibility rule for the same reason — see
// docs/ARCHITECTURE.md §2.3's note on UMG widget Tick.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MidanCountdownWidget.generated.h"

class UTextBlock;

UCLASS()
class MIDANRACE_API UMidanCountdownWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Authored in the WBP_ Blueprint subclass — see docs/MANUAL_STEPS.md
	 *  Phase 7. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CountdownText;

	//~ UUserWidget
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
