#include "MidanTelemetryRingBuffer.h"

#include "MidanLogChannels.h"

void FMidanTelemetryRingBuffer::Initialise(int32 CapacitySamples)
{
	CapacityValue = FMath::Max(CapacitySamples, 1);
	Buffer.SetNum(CapacityValue); // the one allocation; never resized after this
	Head.store(0, std::memory_order_relaxed);
	Tail.store(0, std::memory_order_relaxed);
	bLoggedFullOnce = false;
}

bool FMidanTelemetryRingBuffer::Push(const FMidanTelemetryFrame& Frame)
{
	const int32 CurrentHead = Head.load(std::memory_order_relaxed);
	const int32 NextHead = (CurrentHead + 1) % CapacityValue;

	// Full when advancing Head would collide with Tail — this wastes one
	// slot of capacity in exchange for a lock-free full/empty test with no
	// separate counter, the standard SPSC ring trade.
	if (NextHead == Tail.load(std::memory_order_acquire))
	{
		if (!bLoggedFullOnce)
		{
			UE_LOG(LogMidanTelemetry, Warning, TEXT("FMidanTelemetryRingBuffer: full at capacity %d. Dropping samples until the flush task catches up."), CapacityValue);
			bLoggedFullOnce = true;
		}
		return false;
	}

	Buffer[CurrentHead] = Frame;
	Head.store(NextHead, std::memory_order_release);
	bLoggedFullOnce = false;
	return true;
}

bool FMidanTelemetryRingBuffer::Pop(FMidanTelemetryFrame& OutFrame)
{
	const int32 CurrentTail = Tail.load(std::memory_order_relaxed);
	if (CurrentTail == Head.load(std::memory_order_acquire))
	{
		return false; // empty
	}

	OutFrame = Buffer[CurrentTail];
	Tail.store((CurrentTail + 1) % CapacityValue, std::memory_order_release);
	return true;
}

int32 FMidanTelemetryRingBuffer::DrainInto(TArray<FMidanTelemetryFrame>& OutFrames)
{
	int32 Count = 0;
	FMidanTelemetryFrame Frame;
	while (Pop(Frame))
	{
		OutFrames.Add(Frame);
		++Count;
	}
	return Count;
}

int32 FMidanTelemetryRingBuffer::ApproximateNum() const
{
	const int32 H = Head.load(std::memory_order_relaxed);
	const int32 T = Tail.load(std::memory_order_relaxed);
	return (H >= T) ? (H - T) : (CapacityValue - T + H);
}
