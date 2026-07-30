// Python-callable checkpoint generation, evenly spaced along a track spline.
//
// Responsibility: turn a track spline and a checkpoint count into a
// deterministic, correctly-sequenced set of AMidanCheckpoint actors.
// Single reason to change: how checkpoint placement is derived from the
// spline changes.
//
// The math (even spacing, sector bucketing) is trivial by design — the
// interesting logic (sequence validation, corner-cutting rejection) lives in
// UMidanLapTimingSubsystem where an Automation Spec can reach it, per
// docs/ARCHITECTURE.md §3.6: "each library is a Python-callable entry point
// over math that lives in a runtime module... the editor wrapper is not [worth
// testing], so it must contain no logic worth testing."
//
// Driven by Tools/editor_python/batch_setup_checkpoints.py.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MidanCheckpointGeneratorLibrary.generated.h"

class AMidanTrackSpline;
class AMidanCheckpoint;

UCLASS()
class UMidanCheckpointGeneratorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * (Re)generate checkpoints for a track.
	 *
	 * Destroys every existing AMidanCheckpoint in TrackSpline's level first,
	 * so re-running after editing Count or SectorCount does not accumulate
	 * stale checkpoints from a previous pass — regeneration must be
	 * idempotent, or a designer iterating on checkpoint density silently
	 * doubles up the sequence.
	 *
	 * Checkpoint 0 is always placed at arc length 0 (the finish line).
	 * SectorIndex is assigned by bucketing Count checkpoints evenly across
	 * SectorCount sectors — sector boundaries land on a checkpoint by
	 * construction, never mid-sector.
	 *
	 * Returns false (and generates nothing) if TrackSpline is null, Count < 3,
	 * or SectorCount < 1.
	 */
	UFUNCTION(BlueprintCallable, Category = "Midan|Editor|Track")
	static bool GenerateCheckpoints(AMidanTrackSpline* TrackSpline, int32 Count, int32 SectorCount, TArray<AMidanCheckpoint*>& OutCheckpoints);
};
