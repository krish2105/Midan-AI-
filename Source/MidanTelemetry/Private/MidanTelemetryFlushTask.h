// Async disk flush. Never game-thread file I/O.
//
// Responsibility: write a batch of already-drained frames to disk on a
// background thread.
// Single reason to change: how the background write is scheduled changes.
//
// Private — nothing outside this module should construct one directly.
// UMidanTelemetrySubsystem drains its ring buffers on the game thread (a
// memory copy, not I/O) and hands the resulting array to this task, which
// does the actual FMidanTelemetryBinaryWriter::WriteFrames call off-thread.

#pragma once

#include "CoreMinimal.h"
#include "Async/AsyncWork.h"
#include "MidanTelemetryFrame.h"

class FMidanTelemetryFlushTask : public FNonAbandonableTask
{
public:
	FMidanTelemetryFlushTask(TArray<FMidanTelemetryFrame> InFrames, FString InFilePath, bool bInAppend)
		: Frames(MoveTemp(InFrames))
		, FilePath(MoveTemp(InFilePath))
		, bAppend(bInAppend)
	{
	}

	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMidanTelemetryFlushTask, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	TArray<FMidanTelemetryFrame> Frames;
	FString FilePath;
	bool bAppend;
};
