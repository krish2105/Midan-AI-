#include "UI/MidanHUD.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/AssetManager.h"
#include "EnhancedInputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MidanGameUserSettings.h"
#include "MidanGameplayTags.h"
#include "MidanRacePlayerState.h"
#include "MidanRaceStateInterface.h"
#include "MidanServiceLocator.h"
#include "MidanVehicleInterface.h"
#include "UI/MidanCountdownWidget.h"
#include "UI/MidanHUDDataAsset.h"
#include "UI/MidanMinimapWidget.h"
#include "UI/MidanPauseWidget.h"
#include "UI/MidanResultsWidget.h"

// ---------------------------------------------------------------------------
// UMidanHUDWidget — the persistent race panel
// ---------------------------------------------------------------------------

void UMidanHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!HUDData.IsNull())
	{
		HUDDataLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			HUDData.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &UMidanHUDWidget::OnHUDDataLoaded));
	}
}

void UMidanHUDWidget::OnHUDDataLoaded()
{
	LoadedHUDData = HUDData.Get();
}

void UMidanHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UMidanGameUserSettings* Settings = UMidanGameUserSettings::Get();
	if (Settings && Settings->bPhotoMode)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (Settings)
	{
		SetRenderScale(FVector2D(Settings->HUDScale, Settings->HUDScale));
	}

	const APawn* OwningPawn = GetOwningPlayerPawn();
	const IMidanVehicleInterface* Vehicle = OwningPawn ? Cast<IMidanVehicleInterface>(OwningPawn) : nullptr;
	if (!Vehicle)
	{
		return;
	}

	FMidanVehicleFrameState FrameState;
	Vehicle->GetVehicleFrameState(FrameState);

	if (SpeedText)
	{
		SpeedText->SetText(FText::FromString(FString::Printf(TEXT("%3d"), FMath::RoundToInt(FMath::Abs(FrameState.ForwardSpeedKmh)))));
	}
	if (GearText)
	{
		GearText->SetText(FrameState.Gear == 0 ? FText::FromString(TEXT("N")) : FText::AsNumber(FrameState.Gear));
	}
	if (RPMBar)
	{
		RPMBar->SetPercent(FrameState.EngineRPMNormalised);

		FLinearColor RPMColor = FLinearColor::Green;
		if (LoadedHUDData)
		{
			RPMColor = (FrameState.EngineRPMNormalised >= LoadedHUDData->RPMRedThreshold) ? LoadedHUDData->RPMRedColor
				: (FrameState.EngineRPMNormalised >= LoadedHUDData->RPMAmberThreshold) ? LoadedHUDData->RPMAmberColor
				: LoadedHUDData->RPMNormalColor;
		}
		RPMBar->SetFillColorAndOpacity(RPMColor);
	}

	UMidanServiceLocatorSubsystem* Locator = GetWorld() ? GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>() : nullptr;
	IMidanRaceStateInterface* RaceState = Locator ? Locator->GetRaceState().GetInterface() : nullptr;

	if (RaceState && OwningPawn)
	{
		if (PositionText)
		{
			const int32 Position = RaceState->GetRacerPosition(OwningPawn);
			PositionText->SetText(Position > 0 ? FText::FromString(FString::Printf(TEXT("P%d"), Position)) : FText::FromString(TEXT("--")));
		}
		if (LapText)
		{
			LapText->SetText(FText::AsNumber(RaceState->GetRacerLapCount(OwningPawn) + 1));
		}
	}

	// Sector delta and assist/off-track flags are the "informative, not
	// critical" tier — ART_DIRECTION §8.4 fades these under minimal-HUD.
	const bool bMinimalHUD = Settings && Settings->bMinimalHUD;

	const AMidanRacePlayerState* RacePS = GetOwningPlayerState<AMidanRacePlayerState>();
	if (SectorDeltaText)
	{
		SectorDeltaText->SetVisibility(bMinimalHUD ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (!bMinimalHUD && RacePS && RacePS->bHasSectorDelta)
		{
			// Sign and arrow, never colour alone — ART_DIRECTION §8.4's
			// colourblind rule.
			const bool bFaster = RacePS->LastSectorDeltaSeconds < 0.f;
			SectorDeltaText->SetText(FText::FromString(FString::Printf(
				TEXT("%s%.3f"), bFaster ? TEXT("↓") : TEXT("↑"), FMath::Abs(RacePS->LastSectorDeltaSeconds))));

			if (LoadedHUDData)
			{
				SectorDeltaText->SetColorAndOpacity(bFaster ? LoadedHUDData->SectorDeltaFasterColor : LoadedHUDData->SectorDeltaSlowerColor);
			}
		}
	}

	const ESlateVisibility FlagCollapsedVisibility = bMinimalHUD ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;

	// Momentary, not sticky — ART_DIRECTION §8.5: read straight off this
	// frame's state, never latched.
	if (TCFlagText)
	{
		TCFlagText->SetVisibility(FrameState.bTractionControlActive ? FlagCollapsedVisibility : ESlateVisibility::Collapsed);
		if (LoadedHUDData)
		{
			TCFlagText->SetColorAndOpacity(LoadedHUDData->AssistInterventionColor);
		}
	}
	if (ABSFlagText)
	{
		ABSFlagText->SetVisibility(FrameState.bABSActive ? FlagCollapsedVisibility : ESlateVisibility::Collapsed);
		if (LoadedHUDData)
		{
			ABSFlagText->SetColorAndOpacity(LoadedHUDData->AssistInterventionColor);
		}
	}
	if (OffTrackFlagText)
	{
		const bool bOffTrack = FrameState.GetOffTrackWheelCount() > 0;
		OffTrackFlagText->SetVisibility(bOffTrack ? FlagCollapsedVisibility : ESlateVisibility::Collapsed);
		if (LoadedHUDData)
		{
			OffTrackFlagText->SetColorAndOpacity(LoadedHUDData->OffTrackColor);
		}
	}

	if (Minimap)
	{
		Minimap->SetVisibility(bMinimalHUD ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

// ---------------------------------------------------------------------------
// AMidanHUD
// ---------------------------------------------------------------------------

AMidanHUD::AMidanHUD()
{
}

void AMidanHUD::BeginPlay()
{
	Super::BeginPlay();

	TArray<FSoftObjectPath> Paths;
	if (!MainHUDWidgetClass.IsNull()) Paths.Add(MainHUDWidgetClass.ToSoftObjectPath());
	if (!CountdownWidgetClass.IsNull()) Paths.Add(CountdownWidgetClass.ToSoftObjectPath());
	if (!ResultsWidgetClass.IsNull()) Paths.Add(ResultsWidgetClass.ToSoftObjectPath());
	if (!PauseWidgetClass.IsNull()) Paths.Add(PauseWidgetClass.ToSoftObjectPath());

	if (Paths.Num() > 0)
	{
		WidgetClassesLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			Paths, FStreamableDelegate::CreateUObject(this, &AMidanHUD::OnWidgetClassesLoaded));
	}

	if (!PauseAction.IsNull())
	{
		PauseActionLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			PauseAction.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &AMidanHUD::OnPauseActionLoaded));
	}
}

void AMidanHUD::OnWidgetClassesLoaded()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	if (UClass* MainClass = MainHUDWidgetClass.Get())
	{
		MainHUDWidget = CreateWidget<UMidanHUDWidget>(PC, MainClass);
		if (MainHUDWidget) MainHUDWidget->AddToViewport(0);
	}
	if (UClass* CountdownClass = CountdownWidgetClass.Get())
	{
		CountdownWidget = CreateWidget<UMidanCountdownWidget>(PC, CountdownClass);
		if (CountdownWidget) CountdownWidget->AddToViewport(10);
	}
	if (UClass* ResultsClass = ResultsWidgetClass.Get())
	{
		ResultsWidget = CreateWidget<UMidanResultsWidget>(PC, ResultsClass);
		if (ResultsWidget) ResultsWidget->AddToViewport(10);
	}
	if (UClass* PauseClass = PauseWidgetClass.Get())
	{
		PauseWidget = CreateWidget<UMidanPauseWidget>(PC, PauseClass);
		if (PauseWidget)
		{
			PauseWidget->OnResumeRequested.BindUObject(this, &AMidanHUD::HandleResumeRequested);
			PauseWidget->AddToViewport(20);
			PauseWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void AMidanHUD::OnPauseActionLoaded()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	EnableInput(PC);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (UInputAction* Action = PauseAction.Get())
		{
			EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, &AMidanHUD::TogglePause);
		}
	}
}

void AMidanHUD::TogglePause()
{
	bIsPaused = !bIsPaused;

	if (PauseWidget)
	{
		PauseWidget->SetVisibility(bIsPaused ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (APlayerController* PC = GetOwningPlayerController())
	{
		UGameplayStatics::SetGamePaused(GetWorld(), bIsPaused);

		if (bIsPaused)
		{
			PC->SetInputMode(FInputModeUIOnly());
			PC->bShowMouseCursor = true;
		}
		else
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;
		}
	}
}

void AMidanHUD::HandleResumeRequested()
{
	if (bIsPaused)
	{
		TogglePause();
	}
}

void AMidanHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DisableInput(GetOwningPlayerController());
	Super::EndPlay(EndPlayReason);
}
