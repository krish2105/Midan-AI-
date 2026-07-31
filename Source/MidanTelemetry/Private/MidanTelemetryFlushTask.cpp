#include "MidanTelemetryFlushTask.h"

#include "MidanLogChannels.h"
#include "MidanTelemetryWriter.h"

void FMidanTelemetryFlushTask::DoWork()
{
	if (Frames.Num() == 0)
	{
		return;
	}

	if (!MidanTelemetryWriter::WriteFrames(FilePath, Frames, bAppend))
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("FMidanTelemetryFlushTask: failed to flush %d frames to '%s'."), Frames.Num(), *FilePath);
	}
}
