#include "MidanRacePlayerState.h"

#include "MidanLapTimingSubsystem.h"

AMidanRacePlayerState::AMidanRacePlayerState()
{
}

void AMidanRacePlayerState::BindToLapTiming(AActor* RacerActor)
{
	if (!RacerActor)
	{
		return;
	}

	UMidanLapTimingSubsystem* LapTiming = GetWorld() ? GetWorld()->GetSubsystem<UMidanLapTimingSubsystem>() : nullptr;
	if (!LapTiming)
	{
		return;
	}

	BoundRacer = RacerActor;

	// AddUObject binds unconditionally; every handler below re-checks that the
	// broadcast actor is the one this PlayerState is bound to, since one
	// subsystem instance broadcasts for all eight racers.
	LapTiming->OnLapCompleted.AddUObject(this, &AMidanRacePlayerState::HandleLapCompleted);
	LapTiming->OnLapInvalidated.AddUObject(this, &AMidanRacePlayerState::HandleLapInvalidated);
	LapTiming->OnSectorCompleted.AddUObject(this, &AMidanRacePlayerState::HandleSectorCompleted);
	LapTiming->OnRacerFinished.AddUObject(this, &AMidanRacePlayerState::HandleRacerFinished);
}

void AMidanRacePlayerState::HandleLapCompleted(AActor* Racer, const FMidanLapRecord& Lap)
{
	if (Racer != BoundRacer.Get())
	{
		return;
	}
	LastCompletedLap = Lap;
	CurrentLapIndex = Lap.LapIndex + 1;
	bCurrentLapValid = true;
	if (Lap.bValid && (BestLapTimeSeconds <= 0.f || Lap.TotalTimeSeconds < BestLapTimeSeconds))
	{
		BestLapTimeSeconds = Lap.TotalTimeSeconds;
	}
}

void AMidanRacePlayerState::HandleLapInvalidated(AActor* Racer)
{
	if (Racer != BoundRacer.Get())
	{
		return;
	}
	bCurrentLapValid = false;
}

void AMidanRacePlayerState::HandleSectorCompleted(AActor* Racer, const FMidanSectorTime& Sector, float DeltaVsPreviousBestSeconds)
{
	if (Racer != BoundRacer.Get())
	{
		return;
	}
	CurrentSectorIndex = Sector.SectorIndex + 1;
	LastSectorDeltaSeconds = DeltaVsPreviousBestSeconds;
	bHasSectorDelta = true;
}

void AMidanRacePlayerState::HandleRacerFinished(AActor* Racer, int32 TotalLaps)
{
	if (Racer != BoundRacer.Get())
	{
		return;
	}
	bFinishedRace = true;
}

void AMidanRacePlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UMidanLapTimingSubsystem* LapTiming = World->GetSubsystem<UMidanLapTimingSubsystem>())
		{
			LapTiming->OnLapCompleted.RemoveAll(this);
			LapTiming->OnLapInvalidated.RemoveAll(this);
			LapTiming->OnSectorCompleted.RemoveAll(this);
			LapTiming->OnRacerFinished.RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}
