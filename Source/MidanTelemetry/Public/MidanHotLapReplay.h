// Drives N deterministic hot-lap runs by replaying a recorded ghost, so a
// profiling capture compares identical inputs run to run.
//
// Responsibility: make repeated profiling runs comparable.
// Single reason to change: how a deterministic run is driven or reset
// between repetitions changes.
//
// docs/PERFORMANCE_BUDGET.md §4: "the deterministic hot-lap replay is what
// makes runs comparable across builds. Without it, every performance claim
// is a comparison between two different laps." §4's provenance rule also
// requires a minimum of 3 runs, reporting p95 and max delta — RunCount
// defaults to 3 for exactly that reason, not an arbitrary number.
//
// Lives in MidanTelemetry (not MidanEditorTools) because it drives a
// PLAY-IN-EDITOR-OR-STANDALONE actor through UMidanGhostPlayer, which is a
// runtime component — the editor tool that TRIGGERS this
// (MidanPerfCaptureLibrary, MidanEditorTools) is a thin wrapper, per the
// established "algorithm in a runtime module, editor library is a
// Python-callable entry point" split (docs/ARCHITECTURE.md §3.6).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MidanHotLapReplay.generated.h"

class UMidanGhostPlayer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMidanHotLapRunStarted, int32, RunIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMidanHotLapRunCompleted, int32, RunIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMidanHotLapReplayFinished);

UCLASS(Blueprintable)
class MIDANTELEMETRY_API AMidanHotLapReplay : public AActor
{
	GENERATED_BODY()

public:
	AMidanHotLapReplay();

	/** Absolute or project-relative path to a .midanghost file, authored via
	 *  UMidanGhostRecorder (docs/MANUAL_STEPS.md Phase 9 §9.3). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Midan|Perf")
	FString GhostFilePath;

	/** The vehicle to drive — must already be placed in the level with a
	 *  UMidanGhostPlayer component. Not spawned by this class: which vehicle
	 *  (and which of the three cars) is a scenario choice made by whoever
	 *  sets up the perf-capture map, not something this class should decide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Midan|Perf")
	TSoftObjectPtr<APawn> TargetVehicle;

	/** docs/PERFORMANCE_BUDGET.md §4's minimum run count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Midan|Perf", meta = (ClampMin = "1", ClampMax = "20"))
	int32 RunCount = 3;

	/** How often to poll UMidanGhostPlayer::HasFinished(), seconds. A timer,
	 *  not a Tick — this actor is deliberately absent from
	 *  docs/ARCHITECTURE.md §2.3's tick inventory. Polling a bool every
	 *  100ms costs nothing and needs no per-frame justification. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|Perf", meta = (ClampMin = "0.02", ClampMax = "1.0"))
	float PollIntervalSeconds = 0.1f;

	UPROPERTY(BlueprintAssignable, Category = "Midan|Perf")
	FMidanHotLapRunStarted OnRunStarted;

	UPROPERTY(BlueprintAssignable, Category = "Midan|Perf")
	FMidanHotLapRunCompleted OnRunCompleted;

	/** Fired once after RunCount runs have all completed — the capture
	 *  tool's cue to stop the profiler and finalise the report. */
	UPROPERTY(BlueprintAssignable, Category = "Midan|Perf")
	FMidanHotLapReplayFinished OnReplayFinished;

	UFUNCTION(BlueprintCallable, Category = "Midan|Perf")
	void StartReplay();

	UFUNCTION(BlueprintCallable, Category = "Midan|Perf")
	void StopReplay();

	UFUNCTION(BlueprintPure, Category = "Midan|Perf")
	int32 GetCurrentRunIndex() const { return CurrentRunIndex; }

	UFUNCTION(BlueprintPure, Category = "Midan|Perf")
	bool IsReplaying() const { return bReplaying; }

private:
	void BeginNextRun();
	void PollForRunCompletion();
	void ResetVehicleToStartLine();

	UPROPERTY(Transient)
	TObjectPtr<UMidanGhostPlayer> GhostPlayerComponent;

	FTimerHandle PollTimerHandle;

	int32 CurrentRunIndex = 0;
	bool bReplaying = false;
};
