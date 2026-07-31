"""Run the deterministic hot-lap replay under the CSV Profiler and collect
the resulting per-run CSVs for Tools/analysis/perf_report.py.

Run inside the editor (Play In Editor or -game), with the perf-capture map
open and one AMidanHotLapReplay placed in it:

    <UE>/Engine/Binaries/Mac/UnrealEditor-Cmd \\
        <abs>/Midan.uproject \\
        -run=pythonscript -script="<abs>/Tools/editor_python/capture_perf_baseline.py" \\
        -runs=3

This script contains NO profiling logic — it only finds the
AMidanHotLapReplay actor and calls MidanPerfCaptureLibrary /
AMidanHotLapReplay's own C++ entry points, per docs/ARCHITECTURE.md §3.6's
"editor wrapper is a Python-callable entry point, the algorithm lives in a
runtime module" split. The C++ side (docs/PERFORMANCE_BUDGET.md §4) is what
guarantees each run starts from an identical reset transform — duplicating
that here would risk the two drifting apart.

After this script completes, run:

    python3 Tools/analysis/perf_report.py --csv <each produced CSV> \\
        --config Test --resolution "..." --machine "..." --tier <tier> \\
        --physics-allocations <N> --telemetry-gamethread-io <N> \\
        --editor-module-in-shipping <0|1>
"""

import sys
import unreal


DEFAULT_RUN_COUNT = 3


def find_hot_lap_replay():
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = subsystem.get_all_level_actors()

    replays = [a for a in all_actors if isinstance(a, unreal.MidanHotLapReplay)]

    if not replays:
        return None
    if len(replays) > 1:
        unreal.log_warning(
            f"Found {len(replays)} AMidanHotLapReplay actors; using the first "
            f"({replays[0].get_name()}). A perf-capture map should have exactly one."
        )
    return replays[0]


def main():
    unreal.log("")
    unreal.log("MIDAN - PERF BASELINE CAPTURE")
    unreal.log("")

    run_count = DEFAULT_RUN_COUNT
    for arg in sys.argv:
        if arg.startswith("-runs="):
            run_count = int(arg.split("=", 1)[1])

    replay = find_hot_lap_replay()
    if replay is None:
        unreal.log_error(
            "No AMidanHotLapReplay found in the open level. Place one, set "
            "GhostFilePath and TargetVehicle, and re-run."
        )
        return 1

    replay.set_editor_property("run_count", run_count)

    unreal.log(f"HotLapReplay: {replay.get_name()}  runs={run_count}  ghost={replay.get_editor_property('ghost_file_path')}")

    capture_name = "perf_baseline"
    started = unreal.MidanPerfCaptureLibrary.begin_bracketed_capture(replay, replay, capture_name)
    if not started:
        unreal.log_error("Failed to start the CSV Profiler capture. Is AMidanHotLapReplay's TargetVehicle resolved?")
        return 1

    unreal.log(
        "Capture started. This script does not block on replay completion — "
        "AMidanHotLapReplay.OnReplayFinished fires when all runs are done; "
        "bind it (or watch the Output Log) to know when to call "
        "MidanPerfCaptureLibrary.stop_csv_profiler_capture and collect the CSV "
        "from Saved/Profiling/."
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
