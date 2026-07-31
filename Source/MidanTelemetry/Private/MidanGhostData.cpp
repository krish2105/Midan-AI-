#include "MidanGhostData.h"

#include "MidanLogChannels.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

namespace
{
	// 'MGHO' — distinct from telemetry's magic, so a file of the wrong type
	// is rejected immediately rather than partially misparsed.
	constexpr uint32 GhostMagicNumber = 0x4F48474Du;
}

bool MidanGhostIO::SaveRecording(const FString& FilePath, const FMidanGhostRecording& Recording)
{
	FBufferArchive Archive;
	uint32 Magic = GhostMagicNumber;
	int32 Version = FMidanGhostRecording::CurrentFormatVersion;

	Archive << Magic;
	Archive << Version;
	Archive << const_cast<FMidanGhostRecording&>(Recording); // operator<< is shared read/write

	if (!FFileHelper::SaveArrayToFile(Archive, *FilePath))
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("MidanGhostIO::SaveRecording: failed to save '%s' (%d input frames)."), *FilePath, Recording.InputFrames.Num());
		return false;
	}

	return true;
}

bool MidanGhostIO::LoadRecording(const FString& FilePath, FMidanGhostRecording& OutRecording)
{
	TArray<uint8> RawBytes;
	if (!FFileHelper::LoadFileToArray(RawBytes, *FilePath))
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("MidanGhostIO::LoadRecording: could not load '%s'."), *FilePath);
		return false;
	}

	FMemoryReader Archive(RawBytes, /*bIsPersistent=*/true);

	uint32 Magic = 0;
	int32 Version = 0;
	Archive << Magic;
	if (Magic != GhostMagicNumber)
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("MidanGhostIO::LoadRecording: '%s' is not a Midan ghost file."), *FilePath);
		return false;
	}

	Archive << Version;
	if (Version != FMidanGhostRecording::CurrentFormatVersion)
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("MidanGhostIO::LoadRecording: '%s' is format version %d; this build reads version %d."),
			*FilePath, Version, FMidanGhostRecording::CurrentFormatVersion);
		return false;
	}

	Archive << OutRecording;
	return !Archive.IsError();
}
