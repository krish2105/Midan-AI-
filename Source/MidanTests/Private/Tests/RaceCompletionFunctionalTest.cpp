#include "Tests/RaceCompletionFunctionalTest.h"

#include "MidanGameplayTags.h"
#include "MidanRaceGameState.h"
#include "MidanRacePlayerState.h"
#include "MidanRaceStateInterface.h"
#include "MidanServiceLocator.h"

ARaceCompletionFunctionalTest::ARaceCompletionFunctionalTest()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ARaceCompletionFunctionalTest::StartTest()
{
	Super::StartTest();
	ElapsedSeconds = 0.f;
}

void ARaceCompletionFunctionalTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedSeconds += DeltaSeconds;

	UMidanServiceLocatorSubsystem* Locator = GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>();
	IMidanRaceStateInterface* RaceState = Locator ? Locator->GetRaceState().GetInterface() : nullptr;

	if (RaceState && RaceState->GetRaceStateTag() == MidanTags::Race_State_Results)
	{
		const AMidanRaceGameState* GameState = GetWorld()->GetGameState<AMidanRaceGameState>();
		const int32 RacerCount = GameState ? GameState->PlayerArray.Num() : 0;

		int32 RacersWithAPosition = 0;
		if (GameState)
		{
			for (const APlayerState* PS : GameState->PlayerArray)
			{
				if (const AMidanRacePlayerState* RacePS = Cast<AMidanRacePlayerState>(PS))
				{
					RacersWithAPosition += (RacePS->Position > 0) ? 1 : 0;
				}
			}
		}

		if (RacersWithAPosition == 0)
		{
			FinishTest(EFunctionalTestResult::Failed, TEXT("Reached Race.State.Results but no racer has a computed Position."));
			return;
		}

		FinishTest(EFunctionalTestResult::Succeeded, FString::Printf(
			TEXT("Race reached Results after %.1fs with %d/%d racers positioned."), ElapsedSeconds, RacersWithAPosition, RacerCount));
		return;
	}

	if (ElapsedSeconds >= TimeoutSeconds)
	{
		const FString StateTagName = (RaceState) ? RaceState->GetRaceStateTag().ToString() : TEXT("(no race state registered)");
		FinishTest(EFunctionalTestResult::Failed, FString::Printf(
			TEXT("Race did not reach Results within %.1fs — last observed state: %s."), TimeoutSeconds, *StateTagName));
	}
}
