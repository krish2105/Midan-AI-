#include "UI/MidanMinimapWidget.h"

#include "Engine/AssetManager.h"
#include "MidanRaceGameState.h"
#include "MidanRacePlayerState.h"
#include "MidanServiceLocator.h"
#include "MidanTrackInterface.h"
#include "UI/MidanHUDDataAsset.h"

void UMidanMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!HUDData.IsNull())
	{
		HUDDataLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			HUDData.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &UMidanMinimapWidget::OnHUDDataLoaded));
	}

	RebuildTrackPolyline();
}

void UMidanMinimapWidget::OnHUDDataLoaded()
{
	LoadedHUDData = HUDData.Get();
	RebuildTrackPolyline(); // sample count may have come from the asset
}

void UMidanMinimapWidget::RebuildTrackPolyline()
{
	NormalisedTrackPoints.Reset();

	UMidanServiceLocatorSubsystem* Locator = GetWorld() ? GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>() : nullptr;
	IMidanTrackInterface* Track = Locator ? Locator->GetTrack().GetInterface() : nullptr;
	if (!Track)
	{
		return; // no track registered yet — BeginPlay ordering, or this is a menu-level preview
	}

	const int32 SampleCount = LoadedHUDData ? LoadedHUDData->MinimapSampleCount : 128;
	const float TrackLength = Track->GetTrackLength();
	if (TrackLength <= KINDA_SMALL_NUMBER || SampleCount < 3)
	{
		return;
	}

	TArray<FVector2D> WorldXY;
	WorldXY.Reserve(SampleCount);

	FVector2D Min(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector2D Max(TNumericLimits<float>::Lowest(), TNumericLimits<float>::Lowest());

	for (int32 i = 0; i < SampleCount; ++i)
	{
		const float Distance = (TrackLength * i) / static_cast<float>(SampleCount);
		const FVector Location = Track->GetTransformAtDistance(Distance).GetLocation();
		const FVector2D Point(Location.X, Location.Y);

		WorldXY.Add(Point);
		Min = FVector2D::Min(Min, Point);
		Max = FVector2D::Max(Max, Point);
	}

	const FVector2D Extent = Max - Min;
	const float UniformScale = 1.f / FMath::Max(FMath::Max(Extent.X, Extent.Y), KINDA_SMALL_NUMBER);
	const FVector2D Center = (Min + Max) * 0.5f;

	NormalisedTrackPoints.Reserve(SampleCount);
	for (const FVector2D& Point : WorldXY)
	{
		// Centred at 0.5,0.5, uniformly scaled so the track's aspect ratio
		// is preserved rather than stretched to fill a non-square widget.
		NormalisedTrackPoints.Add(FVector2D(0.5f, 0.5f) + (Point - Center) * UniformScale);
	}
}

int32 UMidanMinimapWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (NormalisedTrackPoints.Num() < 2)
	{
		return LayerId;
	}

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FLinearColor TrackColor = LoadedHUDData ? LoadedHUDData->MinimapTrackColor : FLinearColor::White;

	TArray<FVector2D> LinePoints;
	LinePoints.Reserve(NormalisedTrackPoints.Num() + 1);
	for (const FVector2D& Normalised : NormalisedTrackPoints)
	{
		LinePoints.Add(Normalised * LocalSize);
	}
	LinePoints.Add(LinePoints[0]); // closed loop

	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), LinePoints, ESlateDrawEffect::None, TrackColor, true, 2.f);
	++LayerId;

	// Racer dots. Same module as AMidanRaceGameState, so referencing it
	// directly here is not a cross-module concern — see the class comment.
	if (const AMidanRaceGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AMidanRaceGameState>() : nullptr)
	{
		UMidanServiceLocatorSubsystem* Locator = GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>();
		IMidanTrackInterface* Track = Locator ? Locator->GetTrack().GetInterface() : nullptr;
		const APawn* LocalPawn = GetOwningPlayerPawn();

		if (Track)
		{
			const float TrackLength = Track->GetTrackLength();
			const float PlayerDotSize = 6.f;
			const float OpponentDotSize = 4.f;
			const FLinearColor PlayerColor = LoadedHUDData ? LoadedHUDData->MinimapPlayerDotColor : FLinearColor::Yellow;
			const FLinearColor OpponentColor = LoadedHUDData ? LoadedHUDData->MinimapOpponentDotColor : FLinearColor::Gray;

			for (const APlayerState* PS : GameState->PlayerArray)
			{
				const AMidanRacePlayerState* RacePS = Cast<AMidanRacePlayerState>(PS);
				const AActor* Racer = RacePS ? RacePS->GetTrackedRacerActor() : nullptr;
				if (!Racer)
				{
					continue;
				}

				float LateralOffset = 0.f;
				const float ArcLength = Track->GetClosestDistanceToWorldLocation(Racer->GetActorLocation(), LateralOffset);
				const float Alpha = TrackLength > KINDA_SMALL_NUMBER ? (ArcLength / TrackLength) : 0.f;
				const int32 SampleIndex = FMath::Clamp(FMath::RoundToInt(Alpha * NormalisedTrackPoints.Num()), 0, NormalisedTrackPoints.Num() - 1);
				const FVector2D DotLocal = NormalisedTrackPoints[SampleIndex] * LocalSize;

				const bool bIsLocalPlayer = (Racer == LocalPawn);
				const float DotSize = bIsLocalPlayer ? PlayerDotSize : OpponentDotSize;
				const FLinearColor DotColor = bIsLocalPlayer ? PlayerColor : OpponentColor;

				const FPaintGeometry DotGeometry = AllottedGeometry.ToPaintGeometry(
					FVector2D(DotSize, DotSize), FSlateLayoutTransform(DotLocal - FVector2D(DotSize * 0.5f, DotSize * 0.5f)));

				FSlateDrawElement::MakeBox(OutDrawElements, LayerId, DotGeometry, FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, DotColor);
			}
		}
	}

	return LayerId + 1;
}
