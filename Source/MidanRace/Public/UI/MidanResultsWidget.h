// Final standings, shown during Race.State.Results.
//
// Responsibility: display the finishing order once the race is over.
// Single reason to change: what the results screen shows changes.
//
// Rebuilds its row list only on the Grid/Countdown/Racing -> Results
// transition, not every tick — the standings are frozen the moment the race
// ends, so re-deriving them every frame would be both wasted work and,
// because AMidanRaceGameState's position timer keeps running, actively wrong
// if a straggler crosses the line after the screen is already showing.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MidanResultsWidget.generated.h"

class UVerticalBox;

UCLASS()
class MIDANRACE_API UMidanResultsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** One row is added per racer on entering Results. Authored in the WBP_
	 *  Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UVerticalBox> ResultsList;

	//~ UUserWidget
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildResultsRows();

	bool bResultsBuilt = false;
};
