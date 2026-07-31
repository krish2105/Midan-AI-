#include "MidanTelemetryWriter.h"

#include "HAL/FileManager.h"
#include "MidanLogChannels.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

namespace
{
	// 'MIDA' — arbitrary but stable, so a reader can reject a file that is
	// not one of ours before trusting its byte layout.
	constexpr uint32 MagicNumber = 0x4144494Du;
}

bool MidanTelemetryWriter::WriteFrames(const FString& FilePath, const TArray<FMidanTelemetryFrame>& Frames, bool bAppend)
{
	TArray<FMidanTelemetryFrame> AllFrames;

	if (bAppend && IFileManager::Get().FileExists(*FilePath))
	{
		// Simplicity over throughput: re-reading and rewriting the whole
		// file on each flush is wasteful for a very long capture, but a
		// vertical-slice race (a handful of laps, a handful of flush
		// cycles) never produces a file large enough for that to matter.
		// A true append-in-place format is a straightforward upgrade if
		// capture duration ever grows past this.
		if (!ReadFrames(FilePath, AllFrames))
		{
			UE_LOG(LogMidanTelemetry, Error, TEXT("WriteFrames: append requested but existing file '%s' could not be read. Aborting to avoid data loss."), *FilePath);
			return false;
		}
	}

	AllFrames.Append(Frames);

	FBufferArchive Archive;
	uint32 Magic = MagicNumber;
	int32 Version = MidanTelemetryConstants::CurrentFormatVersion;
	int32 Count = AllFrames.Num();

	Archive << Magic;
	Archive << Version;
	Archive << Count;

	for (FMidanTelemetryFrame& Frame : AllFrames) // non-const: operator<< is shared read/write
	{
		Archive << Frame;
	}

	if (!FFileHelper::SaveArrayToFile(Archive, *FilePath))
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("WriteFrames: failed to save '%s' (%d frames)."), *FilePath, Count);
		return false;
	}

	return true;
}

bool MidanTelemetryWriter::ReadFrames(const FString& FilePath, TArray<FMidanTelemetryFrame>& OutFrames)
{
	TArray<uint8> RawBytes;
	if (!FFileHelper::LoadFileToArray(RawBytes, *FilePath))
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("ReadFrames: could not load '%s'."), *FilePath);
		return false;
	}

	FMemoryReader Archive(RawBytes, /*bIsPersistent=*/true);

	uint32 Magic = 0;
	int32 Version = 0;
	int32 Count = 0;

	Archive << Magic;
	if (Magic != MagicNumber)
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("ReadFrames: '%s' is not a Midan telemetry file (bad magic)."), *FilePath);
		return false;
	}

	Archive << Version;
	if (Version != MidanTelemetryConstants::CurrentFormatVersion)
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("ReadFrames: '%s' is format version %d; this build reads version %d."),
			*FilePath, Version, MidanTelemetryConstants::CurrentFormatVersion);
		return false;
	}

	Archive << Count;
	if (Count < 0)
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("ReadFrames: '%s' reports a negative frame count. Corrupt file."), *FilePath);
		return false;
	}

	OutFrames.Reset();
	OutFrames.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		FMidanTelemetryFrame Frame;
		Archive << Frame;
		OutFrames.Add(Frame);
	}

	return !Archive.IsError();
}

bool MidanTelemetryWriter::ExportFramesToCsv(const FString& FilePath, const TArray<FMidanTelemetryFrame>& Frames)
{
	TArray<FString> Lines;
	Lines.Reserve(Frames.Num() + 1);

	Lines.Add(TEXT(
		"timestamp_s,source_id,lap,sector,track_arc_cm,track_lateral_cm,x,y,z,speed_kmh,rpm,gear,lateral_g,longitudinal_g,chassis_slip_deg,"
		"throttle,brake,steer,handbrake,"
		"slip_ratio_fl,slip_ratio_fr,slip_ratio_rl,slip_ratio_rr,"
		"slip_angle_fl,slip_angle_fr,slip_angle_rl,slip_angle_rr,"
		"surface_fl,surface_fr,surface_rl,surface_rr,"
		"offtrack_fl,offtrack_fr,offtrack_rl,offtrack_rr"));

	for (const FMidanTelemetryFrame& Frame : Frames)
	{
		Lines.Add(FString::Printf(
			TEXT("%.4f,%s,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f,%d,%.3f,%.3f,%.2f,")
			TEXT("%.3f,%.3f,%.3f,%.3f,")
			TEXT("%.4f,%.4f,%.4f,%.4f,")
			TEXT("%.2f,%.2f,%.2f,%.2f,")
			TEXT("%s,%s,%s,%s,")
			TEXT("%d,%d,%d,%d"),
			Frame.TimestampSeconds, *Frame.SourceId.ToString(), Frame.LapIndex, Frame.SectorIndex, Frame.TrackArcLengthCm, Frame.TrackLateralOffsetCm,
			Frame.Location.X, Frame.Location.Y, Frame.Location.Z, Frame.ForwardSpeedKmh, Frame.EngineRPM, Frame.Gear,
			Frame.LateralG, Frame.LongitudinalG, Frame.ChassisSlipAngleDegrees,
			Frame.Throttle, Frame.Brake, Frame.Steer, Frame.Handbrake,
			Frame.Wheels[0].SlipRatio, Frame.Wheels[1].SlipRatio, Frame.Wheels[2].SlipRatio, Frame.Wheels[3].SlipRatio,
			Frame.Wheels[0].SlipAngleDegrees, Frame.Wheels[1].SlipAngleDegrees, Frame.Wheels[2].SlipAngleDegrees, Frame.Wheels[3].SlipAngleDegrees,
			*Frame.Wheels[0].SurfaceTagName.ToString(), *Frame.Wheels[1].SurfaceTagName.ToString(), *Frame.Wheels[2].SurfaceTagName.ToString(), *Frame.Wheels[3].SurfaceTagName.ToString(),
			Frame.Wheels[0].bOffTrack ? 1 : 0, Frame.Wheels[1].bOffTrack ? 1 : 0, Frame.Wheels[2].bOffTrack ? 1 : 0, Frame.Wheels[3].bOffTrack ? 1 : 0));
	}

	if (!FFileHelper::SaveStringArrayToFile(Lines, *FilePath))
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("ExportFramesToCsv: failed to save '%s' (%d frames)."), *FilePath, Frames.Num());
		return false;
	}

	return true;
}
