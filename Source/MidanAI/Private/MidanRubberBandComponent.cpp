#include "MidanRubberBandComponent.h"

#include "AIController.h"
#include "GameFramework/PlayerController.h"
#include "MidanAIDifficultyDataAsset.h"
#include "MidanGameplayTags.h"
#include "MidanMathUtils.h"
#include "MidanRaceStateInterface.h"
#include "MidanServiceLocator.h"
#include "MidanTelemetryInterfaces.h"
#include "MidanTrackInterface.h"
#include "MidanVehicleInterface.h"

UMidanRubberBandComponent::UMidanRubberBandComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMidanRubberBandComponent::Initialise(const UMidanAIDifficultyDataAsset* Difficulty)
{
	if (Difficulty)
	{
		MaxEnvelope = Difficulty->RubberBandMaxEnvelope;
		DecayRate = Difficulty->RubberBandDecayRate;
		TriggerGapSeconds = Difficulty->RubberBandTriggerGapSeconds;
	}
}

float UMidanRubberBandComponent::ComputeGapToPlayerSeconds() const
{
	AAIController* AIController = Cast<AAIController>(GetOwner());
	APawn* SelfPawn = AIController ? AIController->GetPawn() : nullptr;
	IMidanVehicleInterface* SelfVehicle = SelfPawn ? Cast<IMidanVehicleInterface>(SelfPawn) : nullptr;

	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;

	if (!SelfPawn || !SelfVehicle || !PlayerPawn || PlayerPawn == SelfPawn)
	{
		return 0.f;
	}

	UMidanServiceLocatorSubsystem* Locator = GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>();
	IMidanTrackInterface* Track = Locator ? Locator->GetTrack().GetInterface() : nullptr;
	IMidanRaceStateInterface* RaceState = Locator ? Locator->GetRaceState().GetInterface() : nullptr;

	if (!Track || !RaceState)
	{
		return 0.f;
	}

	float SelfLateral = 0.f;
	float PlayerLateral = 0.f;
	const float SelfArc = Track->GetClosestDistanceToWorldLocation(SelfPawn->GetActorLocation(), SelfLateral);
	const float PlayerArc = Track->GetClosestDistanceToWorldLocation(PlayerPawn->GetActorLocation(), PlayerLateral);
	const float TrackLength = Track->GetTrackLength();

	// One-line total-progress formula, duplicated from MidanRace's
	// MidanPositionCalculator rather than depending on it — see the header
	// comment.
	const float SelfProgress = static_cast<float>(RaceState->GetRacerLapCount(SelfPawn)) * TrackLength + SelfArc;
	const float PlayerProgress = static_cast<float>(RaceState->GetRacerLapCount(PlayerPawn)) * TrackLength + PlayerArc;

	FMidanVehicleFrameState SelfFrameState;
	SelfVehicle->GetVehicleFrameState(SelfFrameState);
	const float SelfSpeedCmS = SelfFrameState.LinearVelocity.Size();

	return MidanMath::SafeDivide(PlayerProgress - SelfProgress, FMath::Max(SelfSpeedCmS, 1.f), 0.f);
}

void UMidanRubberBandComponent::UpdateEnvelope(float DeltaTime)
{
	const float GapSeconds = ComputeGapToPlayerSeconds();
	const bool bShouldBoost = GapSeconds > TriggerGapSeconds;
	const float TargetMultiplier = bShouldBoost ? (1.f + MaxEnvelope) : 1.f;

	CurrentMultiplier = MidanMath::ExpDamp(CurrentMultiplier, TargetMultiplier, DecayRate, DeltaTime);

	const bool bIsBoostingNow = CurrentMultiplier > 1.001f;
	if (bIsBoostingNow == bWasBoosting)
	{
		return;
	}
	bWasBoosting = bIsBoostingNow;

	UMidanServiceLocatorSubsystem* Locator = GetWorld() ? GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>() : nullptr;
	if (!Locator || !Locator->IsTelemetryCapturing())
	{
		return;
	}

	IMidanTelemetrySink* Sink = Locator->GetTelemetrySink().GetInterface();
	if (!Sink)
	{
		return;
	}

	AAIController* AIController = Cast<AAIController>(GetOwner());
	AActor* SelfPawn = AIController ? AIController->GetPawn() : nullptr;

	Sink->RecordEvent(MidanTags::Telemetry_Event_RubberBand, SelfPawn, CurrentMultiplier);
}
