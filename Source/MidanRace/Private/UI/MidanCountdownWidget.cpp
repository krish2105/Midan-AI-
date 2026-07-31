#include "UI/MidanCountdownWidget.h"

#include "Components/TextBlock.h"
#include "MidanGameplayTags.h"
#include "MidanRaceStateInterface.h"
#include "MidanServiceLocator.h"

void UMidanCountdownWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UMidanServiceLocatorSubsystem* Locator = GetWorld() ? GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>() : nullptr;
	IMidanRaceStateInterface* RaceState = Locator ? Locator->GetRaceState().GetInterface() : nullptr;

	const bool bIsCountdown = RaceState && RaceState->GetRaceStateTag() == MidanTags::Race_State_Countdown;
	SetVisibility(bIsCountdown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (!bIsCountdown || !CountdownText)
	{
		return;
	}

	const float Remaining = RaceState->GetCountdownRemainingSeconds();
	const FText Display = (Remaining > 0.4f)
		? FText::AsNumber(FMath::CeilToInt(Remaining))
		: FText::FromString(TEXT("GO"));

	CountdownText->SetText(Display);
}
