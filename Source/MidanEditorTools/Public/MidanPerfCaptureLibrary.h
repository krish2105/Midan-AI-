// Python-callable stat and Insights-trace capture triggers.
//
// Responsibility: start/stop the CSV Profiler and Unreal Insights trace that
// bracket a deterministic hot-lap replay.
// Single reason to change: how a capture is started or stopped changes.
//
// Thin by design — the algorithm (what makes runs comparable) lives in
// AMidanHotLapReplay (MidanTelemetry); this library only issues the console
// commands that turn profiling on and off around it. Same split as
// MidanCheckpointGeneratorLibrary and MidanRacingLineToolLibrary
// (docs/ARCHITECTURE.md §3.6).
//
// API VERIFY throughout: the exact CSV Profiler and Trace console command
// syntax is version-sensitive and unconfirmed without an installed engine —
// see docs/ASSUMPTIONS.md.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MidanPerfCaptureLibrary.generated.h"

class AMidanHotLapReplay;

UCLASS()
class UMidanPerfCaptureLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Starts the built-in CSV Profiler, which produces one row per frame
	 *  with per-stat-category timings — the input Tools/analysis/perf_report.py
	 *  parses. CaptureName becomes the output file's base name. */
	UFUNCTION(BlueprintCallable, Category = "Midan|Editor|Perf")
	static bool StartCsvProfilerCapture(UObject* WorldContextObject, const FString& CaptureName);

	UFUNCTION(BlueprintCallable, Category = "Midan|Editor|Perf")
	static bool StopCsvProfilerCapture(UObject* WorldContextObject);

	/** Starts an Unreal Insights trace channel set sufficient to inspect GPU
	 *  pass timings after the fact — a richer, heavier capture than the CSV
	 *  Profiler, used for the "which sub-line is actually over budget"
	 *  investigation rather than every gate run. */
	UFUNCTION(BlueprintCallable, Category = "Midan|Editor|Perf")
	static bool StartInsightsTrace(UObject* WorldContextObject, const FString& CaptureName);

	UFUNCTION(BlueprintCallable, Category = "Midan|Editor|Perf")
	static bool StopInsightsTrace(UObject* WorldContextObject);

	/** Convenience: starts the CSV Profiler, then calls
	 *  AMidanHotLapReplay::StartReplay — bracketing the capture around the
	 *  hot lap exactly, not the grid or countdown either side of it. */
	UFUNCTION(BlueprintCallable, Category = "Midan|Editor|Perf")
	static bool BeginBracketedCapture(UObject* WorldContextObject, AMidanHotLapReplay* Replay, const FString& CaptureName);
};
