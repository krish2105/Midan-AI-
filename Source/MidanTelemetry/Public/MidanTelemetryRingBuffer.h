// Preallocated single-producer/single-consumer ring. Zero allocation in the
// write path.
//
// Responsibility: hold in-flight telemetry samples between the 60Hz capture
// accumulator (producer, game thread) and the flush task (consumer,
// background thread).
// Single reason to change: the buffering strategy itself changes.
//
// SPSC, not general-purpose: exactly one thread pushes (the game thread, via
// UMidanTelemetrySubsystem's accumulator) and exactly one thread pops (the
// background flush task). That constraint is what makes a lock-free
// implementation with plain atomics correct and simple — a general MPMC ring
// would need far more machinery for a case this project never needs.
//
// Capacity is fixed at Initialise and never grows. CLAUDE.md forbids
// allocation in a physics callback; this buffer is filled from the game
// thread at 60Hz, not the physics callback, but the same zero-allocation
// discipline applies for the same reason — a capture running for a whole
// race must not pay for reallocation mid-lap.

#pragma once

#include "CoreMinimal.h"
#include "MidanTelemetryFrame.h"
#include <atomic>

class MIDANTELEMETRY_API FMidanTelemetryRingBuffer
{
public:
	/** One-time allocation. Call before capture starts, never mid-capture. */
	void Initialise(int32 CapacitySamples);

	/** Producer only. False (and the frame is dropped) if the buffer is
	 *  full — the consumer has fallen behind. Logs once per drop streak
	 *  rather than per sample, so a slow disk does not spam the log at 60Hz. */
	bool Push(const FMidanTelemetryFrame& Frame);

	/** Consumer only. False if empty. */
	bool Pop(FMidanTelemetryFrame& OutFrame);

	/** Consumer-side bulk drain: pops everything currently available into
	 *  OutFrames (appended, not cleared first) and returns the count moved.
	 *  This is what the flush task calls rather than looping Pop() itself. */
	int32 DrainInto(TArray<FMidanTelemetryFrame>& OutFrames);

	int32 Capacity() const { return CapacityValue; }

	/** Approximate — a producer/consumer race can make this stale the
	 *  instant it's read, which is expected of a lock-free SPSC queue and
	 *  fine for the diagnostic use this is put to (not for correctness). */
	int32 ApproximateNum() const;

private:
	TArray<FMidanTelemetryFrame> Buffer;
	int32 CapacityValue = 0;

	std::atomic<int32> Head { 0 }; // next write index
	std::atomic<int32> Tail { 0 }; // next read index

	bool bLoggedFullOnce = false;
};
