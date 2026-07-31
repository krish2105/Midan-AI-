#include "MidanPerfCaptureLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "MidanHotLapReplay.h"
#include "MidanLogChannels.h"

namespace
{
	bool ExecConsoleCommand(UObject* WorldContextObject, const FString& Command)
	{
		UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : (GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
		if (!GEngine || !World)
		{
			UE_LOG(LogMidanCore, Error, TEXT("MidanPerfCaptureLibrary: no world/engine to exec '%s' against."), *Command);
			return false;
		}

		// API VERIFY: GEngine->Exec is the standard console-command entry
		// point; the command strings themselves (CsvProfile, Trace.*) are
		// unverified against 5.8's exact syntax — docs/ASSUMPTIONS.md.
		return GEngine->Exec(World, *Command);
	}
}

bool UMidanPerfCaptureLibrary::StartCsvProfilerCapture(UObject* WorldContextObject, const FString& CaptureName)
{
	return ExecConsoleCommand(WorldContextObject, FString::Printf(TEXT("CsvProfile Start -filename=%s"), *CaptureName));
}

bool UMidanPerfCaptureLibrary::StopCsvProfilerCapture(UObject* WorldContextObject)
{
	return ExecConsoleCommand(WorldContextObject, TEXT("CsvProfile Stop"));
}

bool UMidanPerfCaptureLibrary::StartInsightsTrace(UObject* WorldContextObject, const FString& CaptureName)
{
	return ExecConsoleCommand(WorldContextObject, FString::Printf(TEXT("Trace.Start default,gpu,frame -filename=%s"), *CaptureName));
}

bool UMidanPerfCaptureLibrary::StopInsightsTrace(UObject* WorldContextObject)
{
	return ExecConsoleCommand(WorldContextObject, TEXT("Trace.Stop"));
}

bool UMidanPerfCaptureLibrary::BeginBracketedCapture(UObject* WorldContextObject, AMidanHotLapReplay* Replay, const FString& CaptureName)
{
	if (!Replay)
	{
		UE_LOG(LogMidanCore, Error, TEXT("BeginBracketedCapture: Replay is null."));
		return false;
	}

	if (!StartCsvProfilerCapture(WorldContextObject, CaptureName))
	{
		return false;
	}

	// Deliberately does NOT auto-bind Replay->OnReplayFinished to call
	// StopCsvProfilerCapture: this library is a stateless
	// UBlueprintFunctionLibrary with no instance to bind a UFUNCTION
	// delegate target to. The caller (Blueprint or
	// Tools/editor_python/capture_perf_baseline.py) listens for
	// OnReplayFinished itself and calls StopCsvProfilerCapture from there —
	// see docs/MANUAL_STEPS.md Phase 8.
	Replay->StartReplay();
	return true;
}
