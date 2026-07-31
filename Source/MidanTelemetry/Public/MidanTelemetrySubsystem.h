// Fixed 60Hz capture accumulator, decoupled from frame rate. Implements
// IMidanTelemetrySink. Enumerates IMidanTelemetrySource providers.
//
// Responsibility: own the capture lifecycle — start, sample, flush, stop.
// Single reason to change: the capture cadence or lifecycle changes.
//
// Sources are discovered, not push-registered: at StartCapture, this
// subsystem iterates the world for actors implementing IMidanTelemetrySource
// (the same interface-discovery pattern MidanRace and MidanAI already use to
// avoid a reverse module dependency — MidanTelemetry depends on MidanCore
// only, and AMidanVehiclePawn cannot call into this subsystem by type
// without MidanVehicle depending on MidanTelemetry, which the graph does not
// grant). See docs/ASSUMPTIONS.md.
//
// The 60Hz accumulator is an FTimerHandle at
// UMidanDeveloperSettings::GetTelemetryTimestepSeconds(), NOT a Tick — this
// is the literal "decoupled from frame rate" requirement (master prompt
// §6.1, docs/ARCHITECTURE.md §2.3): a variable frame rate must not change
// sample spacing.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MidanTelemetryInterfaces.h"
#include "MidanTelemetryRingBuffer.h"
#include "MidanTelemetrySubsystem.generated.h"

UCLASS()
class MIDANTELEMETRY_API UMidanTelemetrySubsystem : public UGameInstanceSubsystem, public IMidanTelemetrySink
{
	GENERATED_BODY()

public:
	/** Begins a capture: discovers every IMidanTelemetrySource in the world,
	 *  allocates a ring buffer per source, and starts the accumulator and
	 *  flush timers. CaptureName becomes part of each source's output file
	 *  name — see GetCaptureFilePath. No-ops with a warning if already
	 *  capturing. */
	UFUNCTION(BlueprintCallable, Category = "Midan|Telemetry")
	void StartCapture(const FString& CaptureName);

	/** Stops the timers and dispatches one final flush per source. Does not
	 *  block on the background writes completing. */
	UFUNCTION(BlueprintCallable, Category = "Midan|Telemetry")
	void StopCapture();

	/**
	 * Reads a completed capture's binary file back and writes a matching
	 * CSV — for Tools/analysis/telemetry_report.py.
	 *
	 * Deliberately synchronous: this is a one-shot, explicitly-triggered
	 * post-capture export, not part of the continuous 60Hz write path the
	 * "never game-thread I/O" rule targets. Call after StopCapture, once the
	 * background flush tasks have had time to finish (docs/MANUAL_STEPS.md
	 * Phase 9 gives the exact sequencing).
	 */
	UFUNCTION(BlueprintCallable, Category = "Midan|Telemetry")
	bool ExportCaptureToCsv(FName SourceId);

	UFUNCTION(BlueprintPure, Category = "Midan|Telemetry")
	FString GetCaptureFilePath(FName SourceId) const;

	UFUNCTION(BlueprintPure, Category = "Midan|Telemetry")
	FString GetCaptureCsvPath(FName SourceId) const;

	//~ IMidanTelemetrySink
	virtual void RecordEvent(const FGameplayTag& EventTag, const AActor* Instigator, float Value) override;
	virtual bool IsCapturing() const override { return bCapturing; }

	//~ UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	struct FSourceCaptureState
	{
		TScriptInterface<IMidanTelemetrySource> Source;
		FMidanTelemetryRingBuffer RingBuffer;

		/** False on the first flush of a capture (overwrite), true after
		 *  (append) — see MidanTelemetryWriter::WriteFrames's bAppend. */
		bool bHasFlushedOnce = false;
	};

	struct FEventLogEntry
	{
		double TimestampSeconds = 0.0;
		FGameplayTag EventTag;
		FName InstigatorId;
		float Value = 0.f;
	};

	void AccumulatorTick();
	void FlushTick();

	TArray<FSourceCaptureState> Sources;
	TArray<FEventLogEntry> Events;

	FTimerHandle AccumulatorTimerHandle;
	FTimerHandle FlushTimerHandle;

	FString CurrentCaptureName;
	double CaptureStartWorldTimeSeconds = 0.0;
	bool bCapturing = false;
};
