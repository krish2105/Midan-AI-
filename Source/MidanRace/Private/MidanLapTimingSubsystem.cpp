#include "MidanLapTimingSubsystem.h"

#include "Algo/Sort.h"
#include "MidanCheckpoint.h"
#include "MidanGameplayTags.h"
#include "MidanLogChannels.h"
#include "MidanRaceRulesDataAsset.h"
#include "MidanServiceLocator.h"
#include "MidanTelemetryInterfaces.h"
#include "MidanVehicleInterface.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// Pure rules
// ---------------------------------------------------------------------------

FMidanCheckpointCrossResult UMidanLapTimingSubsystem::EvaluateCheckpointCross(
	int32 NextExpectedIndex,
	int32 CrossedCheckpointIndex,
	int32 CheckpointCount)
{
	FMidanCheckpointCrossResult Result;
	Result.NewNextExpectedIndex = NextExpectedIndex;

	if (CheckpointCount <= 0 || CrossedCheckpointIndex != NextExpectedIndex)
	{
		// Out of sequence: either ahead (corner-cut) or behind (reverse
		// driving, or re-crossing one already accepted). Same rejection,
		// same reason — see the class comment.
		Result.Outcome = EMidanCheckpointCrossOutcome::Ignored;
		return Result;
	}

	const int32 Advanced = (CrossedCheckpointIndex + 1) % CheckpointCount;

	if (Advanced == 0)
	{
		// Wrapped: every checkpoint 1..N-1 was reached in order and checkpoint
		// 0 (finish line) has now been crossed again.
		Result.Outcome = EMidanCheckpointCrossOutcome::LapCompleted;
		Result.NewNextExpectedIndex = 1;
	}
	else
	{
		Result.Outcome = EMidanCheckpointCrossOutcome::Progressed;
		Result.NewNextExpectedIndex = Advanced;
	}

	return Result;
}

float UMidanLapTimingSubsystem::AccumulateOffTrackGrace(
	float CurrentGraceTimerSeconds,
	bool bIsOffTrack,
	float DeltaSeconds,
	float GraceLimitSeconds,
	bool& bOutInvalidated)
{
	bOutInvalidated = false;

	if (!bIsOffTrack)
	{
		return 0.f;
	}

	const float NewTimer = CurrentGraceTimerSeconds + FMath::Max(DeltaSeconds, 0.f);
	if (NewTimer >= GraceLimitSeconds)
	{
		bOutInvalidated = true;
	}
	return NewTimer;
}

// ---------------------------------------------------------------------------
// World-scoped wiring
// ---------------------------------------------------------------------------

void UMidanLapTimingSubsystem::InitialiseForRace(const UMidanRaceRulesDataAsset* InRaceRules, const TArray<AMidanCheckpoint*>& InCheckpoints)
{
	RaceRules = InRaceRules;

	SortedCheckpoints.Reset();
	for (AMidanCheckpoint* Checkpoint : InCheckpoints)
	{
		if (Checkpoint)
		{
			SortedCheckpoints.Add(Checkpoint);
		}
	}

	Algo::SortBy(SortedCheckpoints, [](const AMidanCheckpoint* C) { return C->Index; });

	bool bContiguous = true;
	for (int32 i = 0; i < SortedCheckpoints.Num(); ++i)
	{
		if (SortedCheckpoints[i]->Index != i)
		{
			UE_LOG(LogMidanRace, Error,
				TEXT("Checkpoint sequence is not contiguous: expected index %d at sorted position %d, found %d on '%s'. Lap timing will misbehave."),
				i, i, SortedCheckpoints[i]->Index, *SortedCheckpoints[i]->GetName());
			bContiguous = false;
		}
	}

	if (!bContiguous)
	{
		UE_LOG(LogMidanRace, Error, TEXT("UMidanLapTimingSubsystem: checkpoint sequence has gaps or duplicates. Regenerate with MidanCheckpointGeneratorLibrary."));
	}

	for (AMidanCheckpoint* Checkpoint : SortedCheckpoints)
	{
		Checkpoint->OnCheckpointCrossed.AddUObject(this, &UMidanLapTimingSubsystem::HandleCheckpointCrossed);
	}

	UE_LOG(LogMidanRace, Log, TEXT("UMidanLapTimingSubsystem: initialised with %d checkpoints."), SortedCheckpoints.Num());
}

void UMidanLapTimingSubsystem::RegisterRacer(AActor* Racer)
{
	if (!Racer)
	{
		return;
	}

	FMidanRacerLapProgress Progress;
	for (float& BestSector : Progress.BestSectorTimeSeconds)
	{
		BestSector = TNumericLimits<float>::Max();
	}
	RacerProgress.Add(Racer, MoveTemp(Progress));
}

void UMidanLapTimingSubsystem::UnregisterRacer(AActor* Racer)
{
	RacerProgress.Remove(Racer);
}

void UMidanLapTimingSubsystem::StartRace()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	for (TPair<TWeakObjectPtr<AActor>, FMidanRacerLapProgress>& Pair : RacerProgress)
	{
		FMidanRacerLapProgress& Progress = Pair.Value;
		Progress.NextExpectedCheckpointIndex = 1;
		Progress.CurrentLapIndex = 0;
		Progress.CurrentSectorIndex = 0;
		Progress.LapStartRaceTimeSeconds = Now;
		Progress.SectorStartRaceTimeSeconds = Now;
		Progress.OffTrackGraceTimerSeconds = 0.f;
		Progress.bCurrentLapValid = true;
		Progress.bFinishedRace = false;
		Progress.SectorsThisLap.Reset();
	}

	bRaceRunning = true;

	if (UWorld* World = GetWorld())
	{
		const float PollInterval = RaceRules.IsValid() ? RaceRules->OffTrackPollIntervalSeconds : 0.1f;
		World->GetTimerManager().SetTimer(OffTrackPollTimerHandle, this, &UMidanLapTimingSubsystem::PollOffTrackState, PollInterval, true);
	}
}

void UMidanLapTimingSubsystem::StopRace()
{
	bRaceRunning = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OffTrackPollTimerHandle);
	}
}

void UMidanLapTimingSubsystem::HandleCheckpointCrossed(AMidanCheckpoint* Checkpoint, AActor* Racer)
{
	if (!bRaceRunning || !Checkpoint)
	{
		return;
	}

	FMidanRacerLapProgress* Progress = FindProgress(Racer);
	if (!Progress || Progress->bFinishedRace)
	{
		return;
	}

	const FMidanCheckpointCrossResult Result = EvaluateCheckpointCross(
		Progress->NextExpectedCheckpointIndex, Checkpoint->Index, SortedCheckpoints.Num());

	if (Result.Outcome == EMidanCheckpointCrossOutcome::Ignored)
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// Sector completion is orthogonal to sequence progress — see the enum
	// comment in the header — so it is checked here against live checkpoint
	// data rather than inside the pure rule.
	if (Checkpoint->SectorIndex != Progress->CurrentSectorIndex)
	{
		FMidanSectorTime SectorTime;
		SectorTime.SectorIndex = Progress->CurrentSectorIndex;
		SectorTime.TimeSeconds = Now - Progress->SectorStartRaceTimeSeconds;
		SectorTime.bValid = Progress->bCurrentLapValid;

		Progress->SectorsThisLap.Add(SectorTime);

		if (SectorTime.bValid && MidanRaceConstants::MaxSectorCount > SectorTime.SectorIndex
			&& SectorTime.TimeSeconds < Progress->BestSectorTimeSeconds[SectorTime.SectorIndex])
		{
			Progress->BestSectorTimeSeconds[SectorTime.SectorIndex] = SectorTime.TimeSeconds;
		}

		Progress->CurrentSectorIndex = Checkpoint->SectorIndex;
		Progress->SectorStartRaceTimeSeconds = Now;

		OnSectorCompleted.Broadcast(Racer, SectorTime);
	}

	Progress->NextExpectedCheckpointIndex = Result.NewNextExpectedIndex;
	Progress->LastValidCheckpointArcLength = Checkpoint->ArcLength;

	if (Result.Outcome != EMidanCheckpointCrossOutcome::LapCompleted)
	{
		return;
	}

	FMidanLapRecord Lap;
	Lap.LapIndex = Progress->CurrentLapIndex;
	Lap.TotalTimeSeconds = Now - Progress->LapStartRaceTimeSeconds;
	Lap.bValid = Progress->bCurrentLapValid;
	Lap.RaceTimeAtCompletionSeconds = Now;
	Lap.SectorCount = FMath::Min(Progress->SectorsThisLap.Num(), MidanRaceConstants::MaxSectorCount);
	for (int32 i = 0; i < Lap.SectorCount; ++i)
	{
		Lap.Sectors[i] = Progress->SectorsThisLap[i];
	}

	if (Lap.bValid && Lap.TotalTimeSeconds < Progress->BestLapTimeSeconds)
	{
		Progress->BestLapTimeSeconds = Lap.TotalTimeSeconds;
	}

	Progress->LastCompletedLap = Lap;
	Progress->CurrentLapIndex += 1;
	Progress->SectorsThisLap.Reset();
	Progress->SectorStartRaceTimeSeconds = Now;
	Progress->LapStartRaceTimeSeconds = Now;
	Progress->bCurrentLapValid = true;

	OnLapCompleted.Broadcast(Racer, Lap);

	const int32 RequiredLaps = RaceRules.IsValid() ? RaceRules->LapCount : 1;
	if (Progress->CurrentLapIndex >= RequiredLaps)
	{
		Progress->bFinishedRace = true;
		OnRacerFinished.Broadcast(Racer, Progress->CurrentLapIndex);
	}
}

void UMidanLapTimingSubsystem::PollOffTrackState()
{
	if (!RaceRules.IsValid())
	{
		return;
	}

	const float PollInterval = RaceRules->OffTrackPollIntervalSeconds;
	const int32 Threshold = RaceRules->OffTrackWheelThreshold;
	const float GraceLimit = RaceRules->OffTrackGraceSeconds;

	UMidanServiceLocatorSubsystem* Locator = GetWorld() ? GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>() : nullptr;

	for (TPair<TWeakObjectPtr<AActor>, FMidanRacerLapProgress>& Pair : RacerProgress)
	{
		AActor* Racer = Pair.Key.Get();
		FMidanRacerLapProgress& Progress = Pair.Value;

		if (!Racer || Progress.bFinishedRace || !Progress.bCurrentLapValid)
		{
			continue;
		}

		const IMidanVehicleInterface* Vehicle = Cast<IMidanVehicleInterface>(Racer);
		if (!Vehicle)
		{
			continue;
		}

		FMidanVehicleFrameState FrameState;
		Vehicle->GetVehicleFrameState(FrameState);
		const bool bIsOffTrack = FrameState.GetOffTrackWheelCount() >= Threshold;

		bool bInvalidated = false;
		Progress.OffTrackGraceTimerSeconds = AccumulateOffTrackGrace(
			Progress.OffTrackGraceTimerSeconds, bIsOffTrack, PollInterval, GraceLimit, bInvalidated);

		if (bInvalidated)
		{
			Progress.bCurrentLapValid = false;
			OnLapInvalidated.Broadcast(Racer);

			if (Locator && Locator->IsTelemetryCapturing())
			{
				if (IMidanTelemetrySink* Sink = Locator->GetTelemetrySink().GetInterface())
				{
					Sink->RecordEvent(MidanTags::Telemetry_Event_LapInvalidated, Racer, Progress.OffTrackGraceTimerSeconds);
				}
			}
		}
	}
}

int32 UMidanLapTimingSubsystem::GetRacerLapIndex(const AActor* Racer) const
{
	const FMidanRacerLapProgress* Progress = FindProgress(Racer);
	return Progress ? Progress->CurrentLapIndex : 0;
}

int32 UMidanLapTimingSubsystem::GetRacerSectorIndex(const AActor* Racer) const
{
	const FMidanRacerLapProgress* Progress = FindProgress(Racer);
	return Progress ? Progress->CurrentSectorIndex : 0;
}

bool UMidanLapTimingSubsystem::IsRacerLapValid(const AActor* Racer) const
{
	const FMidanRacerLapProgress* Progress = FindProgress(Racer);
	return Progress ? Progress->bCurrentLapValid : false;
}

bool UMidanLapTimingSubsystem::IsRacerFinished(const AActor* Racer) const
{
	const FMidanRacerLapProgress* Progress = FindProgress(Racer);
	return Progress ? Progress->bFinishedRace : false;
}

float UMidanLapTimingSubsystem::GetRacerBestLapTimeSeconds(const AActor* Racer) const
{
	const FMidanRacerLapProgress* Progress = FindProgress(Racer);
	return Progress ? Progress->BestLapTimeSeconds : TNumericLimits<float>::Max();
}

FMidanLapRecord UMidanLapTimingSubsystem::GetRacerLastCompletedLap(const AActor* Racer) const
{
	const FMidanRacerLapProgress* Progress = FindProgress(Racer);
	return Progress ? Progress->LastCompletedLap : FMidanLapRecord();
}

float UMidanLapTimingSubsystem::GetRacerLastCheckpointArcLength(const AActor* Racer) const
{
	const FMidanRacerLapProgress* Progress = FindProgress(Racer);
	return Progress ? Progress->LastValidCheckpointArcLength : 0.f;
}

FMidanRacerLapProgress* UMidanLapTimingSubsystem::FindProgress(const AActor* Racer)
{
	return Racer ? RacerProgress.Find(const_cast<AActor*>(Racer)) : nullptr;
}

const FMidanRacerLapProgress* UMidanLapTimingSubsystem::FindProgress(const AActor* Racer) const
{
	return Racer ? RacerProgress.Find(const_cast<AActor*>(Racer)) : nullptr;
}

void UMidanLapTimingSubsystem::Deinitialize()
{
	StopRace();

	for (AMidanCheckpoint* Checkpoint : SortedCheckpoints)
	{
		if (Checkpoint)
		{
			Checkpoint->OnCheckpointCrossed.RemoveAll(this);
		}
	}

	RacerProgress.Empty();
	SortedCheckpoints.Empty();

	Super::Deinitialize();
}
