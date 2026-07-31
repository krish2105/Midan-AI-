#include "UI/MidanResultsWidget.h"

#include "Algo/Sort.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "MidanGameplayTags.h"
#include "MidanRaceStateInterface.h"
#include "MidanRacePlayerState.h"
#include "MidanServiceLocator.h"

void UMidanResultsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UMidanServiceLocatorSubsystem* Locator = GetWorld() ? GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>() : nullptr;
	IMidanRaceStateInterface* RaceState = Locator ? Locator->GetRaceState().GetInterface() : nullptr;
	const bool bIsResults = RaceState && RaceState->GetRaceStateTag() == MidanTags::Race_State_Results;

	SetVisibility(bIsResults ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (bIsResults && !bResultsBuilt)
	{
		BuildResultsRows();
		bResultsBuilt = true;
	}
	else if (!bIsResults)
	{
		// Reset for the next race — a rematch must not show the previous
		// race's frozen standings.
		bResultsBuilt = false;
	}
}

void UMidanResultsWidget::BuildResultsRows()
{
	if (!ResultsList)
	{
		return;
	}
	ResultsList->ClearChildren();

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	TArray<const AMidanRacePlayerState*> Sorted;
	for (const APlayerState* PS : GameState->PlayerArray)
	{
		if (const AMidanRacePlayerState* RacePS = Cast<AMidanRacePlayerState>(PS))
		{
			Sorted.Add(RacePS);
		}
	}

	Algo::SortBy(Sorted, [](const AMidanRacePlayerState* PS) { return PS->Position > 0 ? PS->Position : TNumericLimits<int32>::Max(); });

	for (const AMidanRacePlayerState* RacePS : Sorted)
	{
		const FString BestLap = (RacePS->BestLapTimeSeconds > 0.f)
			? FString::Printf(TEXT("%.3f"), RacePS->BestLapTimeSeconds)
			: TEXT("--");

		UTextBlock* Row = NewObject<UTextBlock>(this);
		Row->SetText(FText::FromString(FString::Printf(
			TEXT("P%d  %s  best %s"), RacePS->Position, *RacePS->GetPlayerName(), *BestLap)));

		ResultsList->AddChildToVerticalBox(Row);
	}
}
