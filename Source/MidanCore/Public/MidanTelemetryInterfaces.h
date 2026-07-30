// Telemetry capture-pull and event-push contracts.
//
// Responsibility: the boundary between things that produce telemetry and the
// thing that records it.
// Single reason to change: the capture or event model changes.
//
// Two interfaces, deliberately separate:
//
//   IMidanTelemetrySource — implemented by anything with recordable state.
//       AMidanVehiclePawn implements it, which is how MidanTelemetry captures
//       vehicles without depending on MidanVehicle (docs/ASSUMPTIONS.md A8).
//
//   IMidanTelemetrySink — implemented by the telemetry subsystem.
//       MidanAI pushes rubber-band events through it without depending on
//       MidanTelemetry (docs/ASSUMPTIONS.md A10).
//
// Splitting them means a producer never gains the ability to read the recording,
// and the recorder never gains the ability to drive a producer.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "MidanCoreTypes.h"
#include "MidanTelemetryInterfaces.generated.h"

UINTERFACE(MinimalAPI)
class UMidanTelemetrySource : public UInterface
{
	GENERATED_BODY()
};

class MIDANCORE_API IMidanTelemetrySource
{
	GENERATED_BODY()

public:
	/**
	 * Fill the current sample. Called at a fixed 60Hz, decoupled from frame
	 * rate (master prompt §6.1).
	 *
	 * Must be cheap and must not allocate: it runs once per source per sample,
	 * so eight vehicles at 60Hz is 480 calls per second.
	 */
	virtual void CaptureTelemetryState(FMidanVehicleFrameState& OutState) const = 0;

	/** Input applied at this sample. Recorded alongside physics state because
	 *  ghost replay replays INPUTS, not transforms (master prompt §6.2) — so
	 *  the inputs are the primary record and the state is the resync check. */
	virtual void CaptureTelemetryInput(FMidanVehicleInputState& OutInput) const = 0;

	/** Stable identifier for this source across a session. Distinguishes the
	 *  player from seven opponents in the capture file. */
	virtual FName GetTelemetrySourceId() const = 0;

	/** False to skip this source — a source that has finished the race or been
	 *  destroyed should stop consuming capture bandwidth. */
	virtual bool IsTelemetryCaptureEnabled() const = 0;
};

UINTERFACE(MinimalAPI)
class UMidanTelemetrySink : public UInterface
{
	GENERATED_BODY()
};

class MIDANCORE_API IMidanTelemetrySink
{
	GENERATED_BODY()

public:
	/**
	 * Record a discrete event with one scalar payload.
	 *
	 * Deliberately minimal — a tag plus a float, no string formatting and no
	 * variadic payload. Master prompt §4.4 requires every rubber-band
	 * application logged so its subtlety is provable, and that is a tag and a
	 * magnitude. A richer event API would invite game-thread string work in a
	 * hot path.
	 */
	virtual void RecordEvent(const FGameplayTag& EventTag, const AActor* Instigator, float Value) = 0;

	/**
	 * True when capture is active.
	 *
	 * Callers MUST check this before building an event. Capture is normally off
	 * in a Shipping build, and an unchecked caller pays for work that is
	 * discarded.
	 */
	virtual bool IsCapturing() const = 0;
};
