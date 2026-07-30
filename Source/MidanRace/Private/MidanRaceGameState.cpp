#include "MidanRaceGameState.h"

#include "MidanGameplayTags.h"
#include "MidanLapTimingSubsystem.h"
#include "MidanPositionCalculator.h"
#include "MidanRacePlayerState.h"
#include "MidanRaceRulesDataAsset.h"
#include "MidanServiceLocator.h"
#include "MidanTrackInterface.h"
#include "TimerManager.h"

AMidanRaceGameState::AMidanRaceGameState()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentStateTag = MidanTags::Race_State_Grid;
}

void AMidanRaceGameState::BeginPlay()
{
	Super::BeginPlay();

	// Registers so MidanAI (Phase 6) can read race phase through
	// IMidanRaceStateInterface without MidanAI depending on MidanRace —
	// docs/ASSUMPTIONS.md A9. Same registration pattern as AMidanTrackSpline.
	if (UMidanServiceLocatorSubsystem* Locator = GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>())
	{
		Locator->RegisterRaceState(TScriptInterface<IMidanRaceStateInterface>(this));
	}
}

void AMidanRaceGameState::InitialiseRaceRules(const UMidanRaceRulesDataAsset* InRaceRules)
{
	RaceRules = InRaceRules;

	const float IntervalSeconds = (RaceRules && RaceRules->PositionUpdateHz > 0.f)
		? (1.f / RaceRules->PositionUpdateHz)
		: (1.f / 10.f);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(PositionUpdateTimerHandle, this, &AMidanRaceGameState::RecomputePositions, IntervalSeconds, true);
	}
}

void AMidanRaceGameState::SetRaceStateTag(FGameplayTag NewState)
{
	CurrentStateTag = NewState;
}

void AMidanRaceGameState::BeginCountdown(float DurationSeconds)
{
	CountdownEndsAtWorldTimeSeconds = GetWorld() ? (GetWorld()->GetTimeSeconds() + FMath::Max(DurationSeconds, 0.f)) : -1.f;
}

void AMidanRaceGameState::MarkRaceStarted()
{
	RaceStartWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	bRaceFinished = false;
}

void AMidanRaceGameState::MarkRaceFinished()
{
	RaceElapsedSecondsAtFinish = GetRaceElapsedSeconds();
	bRaceFinished = true;
}

float AMidanRaceGameState::GetRaceElapsedSeconds() const
{
	if (bRaceFinished)
	{
		return RaceElapsedSecondsAtFinish;
	}
	if (RaceStartWorldTimeSeconds < 0.f || !GetWorld())
	{
		return 0.f;
	}
	return GetWorld()->GetTimeSeconds() - RaceStartWorldTimeSeconds;
}

bool AMidanRaceGameState::IsRacingActive() const
{
	return CurrentStateTag == MidanTags::Race_State_Racing;
}

int32 AMidanRaceGameState::GetRacerPosition(const AActor* Racer) const
{
	for (const APlayerState* PS : PlayerArray)
	{
		if (const AMidanRacePlayerState* RacePS = Cast<AMidanRacePlayerState>(PS))
		{
			if (RacePS->GetTrackedRacerActor() == Racer)
			{
				return RacePS->Position;
			}
		}
	}
	return 0;
}

int32 AMidanRaceGameState::GetRacerLapCount(const AActor* Racer) const
{
	const UMidanLapTimingSubsystem* LapTiming = GetWorld() ? GetWorld()->GetSubsystem<UMidanLapTimingSubsystem>() : nullptr;
	return LapTiming ? LapTiming->GetRacerLapIndex(Racer) : 0;
}

int32 AMidanRaceGameState::GetRacerCount() const
{
	return PlayerArray.Num();
}

float AMidanRaceGameState::GetCountdownRemainingSeconds() const
{
	if (CurrentStateTag != MidanTags::Race_State_Countdown || CountdownEndsAtWorldTimeSeconds < 0.f || !GetWorld())
	{
		return 0.f;
	}
	return FMath::Max(0.f, CountdownEndsAtWorldTimeSeconds - GetWorld()->GetTimeSeconds());
}

void AMidanRaceGameState::RecomputePositions()
{
	UMidanServiceLocatorSubsystem* Locator = GetWorld() ? GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>() : nullptr;
	IMidanTrackInterface* Track = Locator ? Locator->GetTrack().GetInterface() : nullptr;
	UMidanLapTimingSubsystem* LapTiming = GetWorld() ? GetWorld()->GetSubsystem<UMidanLapTimingSubsystem>() : nullptr;

	if (!Track || !LapTiming)
	{
		return;
	}

	const float TrackLength = Track->GetTrackLength();

	TArray<AMidanRacePlayerState*> Racers;
	TArray<float> Progress;
	Racers.Reserve(PlayerArray.Num());
	Progress.Reserve(PlayerArray.Num());

	for (APlayerState* PS : PlayerArray)
	{
		AMidanRacePlayerState* RacePS = Cast<AMidanRacePlayerState>(PS);
		AActor* RacerActor = RacePS ? RacePS->GetTrackedRacerActor() : nullptr;
		if (!RacePS || !RacerActor)
		{
			continue;
		}

		float LateralOffset = 0.f;
		const float ArcLength = Track->GetClosestDistanceToWorldLocation(RacerActor->GetActorLocation(), LateralOffset);
		const int32 LapIndex = LapTiming->GetRacerLapIndex(RacerActor);

		Racers.Add(RacePS);
		Progress.Add(MidanRacePosition::ComputeTotalProgress(LapIndex, ArcLength, TrackLength));
	}

	const TArray<int32> Positions = MidanRacePosition::ComputeRacePositions(Progress);
	for (int32 i = 0; i < Racers.Num(); ++i)
	{
		Racers[i]->SetPosition(Positions[i]);
	}
}

void AMidanRaceGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PositionUpdateTimerHandle);

		if (UMidanServiceLocatorSubsystem* Locator = World->GetSubsystem<UMidanServiceLocatorSubsystem>())
		{
			Locator->UnregisterRaceState();
		}
	}
	Super::EndPlay(EndPlayReason);
}
