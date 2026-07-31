#include "MidanTelemetrySubsystem.h"

#include "Async/AsyncWork.h"
#include "EngineUtils.h"
#include "HAL/PlatformFilemanager.h"
#include "MidanDeveloperSettings.h"
#include "MidanLogChannels.h"
#include "MidanRaceStateInterface.h"
#include "MidanServiceLocator.h"
#include "MidanTelemetryFlushTask.h"
#include "MidanTelemetryWriter.h"
#include "MidanTrackInterface.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

void UMidanTelemetrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UMidanTelemetrySubsystem::Deinitialize()
{
	if (bCapturing)
	{
		StopCapture();
	}
	Super::Deinitialize();
}

void UMidanTelemetrySubsystem::StartCapture(const FString& CaptureName)
{
	if (bCapturing)
	{
		UE_LOG(LogMidanTelemetry, Warning, TEXT("StartCapture('%s') ignored: a capture ('%s') is already running."), *CaptureName, *CurrentCaptureName);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("StartCapture: no world."));
		return;
	}

	const UMidanDeveloperSettings* DevSettings = UMidanDeveloperSettings::Get();
	const int32 RingBufferCapacity = DevSettings ? DevSettings->TelemetryRingBufferSamples : 7200;
	const float TimestepSeconds = DevSettings ? static_cast<float>(DevSettings->GetTelemetryTimestepSeconds()) : (1.f / 60.f);

	Sources.Reset();
	Events.Reset();

	// Discovery, not push-registration — see the class comment.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->Implements<UMidanTelemetrySource>())
		{
			continue;
		}

		FSourceCaptureState State;
		State.Source = TScriptInterface<IMidanTelemetrySource>(Actor);
		State.RingBuffer.Initialise(RingBufferCapacity);
		Sources.Add(MoveTemp(State));
	}

	if (Sources.Num() == 0)
	{
		UE_LOG(LogMidanTelemetry, Warning, TEXT("StartCapture('%s'): no IMidanTelemetrySource actors found in the world."), *CaptureName);
	}

	CurrentCaptureName = CaptureName;
	CaptureStartWorldTimeSeconds = World->GetTimeSeconds();
	bCapturing = true;

	World->GetTimerManager().SetTimer(AccumulatorTimerHandle, this, &UMidanTelemetrySubsystem::AccumulatorTick, TimestepSeconds, true);

	// Flush cadence is independent of and much coarser than the capture
	// cadence — draining every 2s keeps the ring buffers from ever
	// approaching full during a normal lap without dispatching a background
	// task 60 times a second.
	constexpr float FlushIntervalSeconds = 2.f;
	World->GetTimerManager().SetTimer(FlushTimerHandle, this, &UMidanTelemetrySubsystem::FlushTick, FlushIntervalSeconds, true);

	UE_LOG(LogMidanTelemetry, Log, TEXT("StartCapture('%s'): %d source(s), ring capacity %d, %.1fHz."),
		*CaptureName, Sources.Num(), RingBufferCapacity, 1.f / TimestepSeconds);
}

void UMidanTelemetrySubsystem::AccumulatorTick()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMidanServiceLocatorSubsystem* Locator = World->GetSubsystem<UMidanServiceLocatorSubsystem>();
	IMidanTrackInterface* Track = Locator ? Locator->GetTrack().GetInterface() : nullptr;
	IMidanRaceStateInterface* RaceState = Locator ? Locator->GetRaceState().GetInterface() : nullptr;

	const double Now = World->GetTimeSeconds() - CaptureStartWorldTimeSeconds;

	for (FSourceCaptureState& State : Sources)
	{
		IMidanTelemetrySource* Source = State.Source.GetInterface();
		if (!Source || !Source->IsTelemetryCaptureEnabled())
		{
			continue;
		}

		FMidanVehicleFrameState VehicleState;
		Source->CaptureTelemetryState(VehicleState);

		FMidanVehicleInputState Input;
		Source->CaptureTelemetryInput(Input);

		float ArcLengthCm = 0.f;
		float LateralOffsetCm = 0.f;
		int32 LapIndex = 0;
		int32 SectorIndex = 0;

		if (const AActor* SourceActor = Cast<AActor>(State.Source.GetObject()))
		{
			if (Track)
			{
				ArcLengthCm = Track->GetClosestDistanceToWorldLocation(SourceActor->GetActorLocation(), LateralOffsetCm);
			}
			if (RaceState)
			{
				LapIndex = RaceState->GetRacerLapCount(SourceActor);
				SectorIndex = RaceState->GetRacerSectorIndex(SourceActor);
			}
		}

		FMidanTelemetryFrame Frame;
		Frame.BuildFromSource(VehicleState, Input, Now, ArcLengthCm, LateralOffsetCm, LapIndex, SectorIndex, Source->GetTelemetrySourceId());

		State.RingBuffer.Push(Frame);
	}
}

void UMidanTelemetrySubsystem::FlushTick()
{
	for (FSourceCaptureState& State : Sources)
	{
		IMidanTelemetrySource* Source = State.Source.GetInterface();
		if (!Source)
		{
			continue;
		}

		TArray<FMidanTelemetryFrame> Drained;
		if (State.RingBuffer.DrainInto(Drained) == 0)
		{
			continue;
		}

		const FString FilePath = GetCaptureFilePath(Source->GetTelemetrySourceId());
		(new FAutoDeleteAsyncTask<FMidanTelemetryFlushTask>(MoveTemp(Drained), FilePath, State.bHasFlushedOnce))->StartBackgroundTask();
		State.bHasFlushedOnce = true;
	}
}

void UMidanTelemetrySubsystem::StopCapture()
{
	if (!bCapturing)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AccumulatorTimerHandle);
		World->GetTimerManager().ClearTimer(FlushTimerHandle);
	}

	FlushTick(); // final drain

	UE_LOG(LogMidanTelemetry, Log, TEXT("StopCapture('%s'): %d event(s) recorded during capture."), *CurrentCaptureName, Events.Num());

	bCapturing = false;
}

bool UMidanTelemetrySubsystem::ExportCaptureToCsv(FName SourceId)
{
	TArray<FMidanTelemetryFrame> Frames;
	if (!MidanTelemetryWriter::ReadFrames(GetCaptureFilePath(SourceId), Frames))
	{
		return false;
	}
	return MidanTelemetryWriter::ExportFramesToCsv(GetCaptureCsvPath(SourceId), Frames);
}

FString UMidanTelemetrySubsystem::GetCaptureFilePath(FName SourceId) const
{
	return FPaths::ProjectSavedDir() / TEXT("Telemetry") / FString::Printf(TEXT("%s_%s.midantelem"), *CurrentCaptureName, *SourceId.ToString());
}

FString UMidanTelemetrySubsystem::GetCaptureCsvPath(FName SourceId) const
{
	return FPaths::ProjectSavedDir() / TEXT("Telemetry") / FString::Printf(TEXT("%s_%s.csv"), *CurrentCaptureName, *SourceId.ToString());
}

void UMidanTelemetrySubsystem::RecordEvent(const FGameplayTag& EventTag, const AActor* Instigator, float Value)
{
	if (!bCapturing)
	{
		return;
	}

	FEventLogEntry Entry;
	Entry.TimestampSeconds = GetWorld() ? (GetWorld()->GetTimeSeconds() - CaptureStartWorldTimeSeconds) : 0.0;
	Entry.EventTag = EventTag;
	Entry.InstigatorId = Instigator ? Instigator->GetFName() : NAME_None;
	Entry.Value = Value;

	Events.Add(MoveTemp(Entry));
}
