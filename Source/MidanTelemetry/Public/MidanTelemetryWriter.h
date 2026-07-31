// Versioned compact binary read/write, plus a CSV export path for the Python
// tooling in Tools/analysis.
//
// Responsibility: turn a frame array into bytes on disk, and back.
// Single reason to change: the on-disk format changes.
//
// FMidanTelemetryBinaryWriter is the ONLY thing that touches disk during a
// capture, and it is only ever called from FMidanTelemetryFlushTask's
// background thread — "never game-thread file I/O" (master prompt §6.1,
// docs/ARCHITECTURE.md §3.5). The reader exists for the same reason
// TelemetryRoundTripSpec exists: a write path with no read path to verify it
// against is unverifiable.
//
// CSV, not the binary format, is what Tools/analysis/telemetry_report.py
// consumes — a stable, self-describing text format is worth the size cost
// for tooling that needs to stay simple and dependency-free (master prompt
// §6.3, no external Python packages).

#pragma once

#include "CoreMinimal.h"
#include "MidanTelemetryFrame.h"

namespace MidanTelemetryWriter
{
	/** Writes CapacityValue-independent frames to FilePath. bAppend adds to
	 *  an existing file's frame array (used when a capture spans multiple
	 *  flush cycles); false overwrites. Returns false on any I/O failure. */
	MIDANTELEMETRY_API bool WriteFrames(const FString& FilePath, const TArray<FMidanTelemetryFrame>& Frames, bool bAppend);

	/** Reads every frame from FilePath. Returns false (OutFrames untouched)
	 *  if the file is missing, unreadable, or its format version does not
	 *  match MidanTelemetryConstants::CurrentFormatVersion. */
	MIDANTELEMETRY_API bool ReadFrames(const FString& FilePath, TArray<FMidanTelemetryFrame>& OutFrames);

	/**
	 * Exports frames as CSV: one row per frame, one column per scalar field
	 * (wheel fields suffixed _FL/_FR/_RL/_RR). Header row names every column
	 * so the format is self-describing without a schema doc — Tools/analysis
	 * reads column names, not fixed positions, so adding a field here never
	 * breaks the Python side silently.
	 */
	MIDANTELEMETRY_API bool ExportFramesToCsv(const FString& FilePath, const TArray<FMidanTelemetryFrame>& Frames);
}
