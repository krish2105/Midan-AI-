"""Generate checkpoints for the Midan circuit from the editor.

Run inside the editor (Output Log -> Python), or headless:

    <UE>/Engine/Binaries/Mac/UnrealEditor-Cmd \\
        <abs>/Midan.uproject \\
        -run=pythonscript -script="<abs>/Tools/editor_python/batch_setup_checkpoints.py"

Requires the map with the track already open (or loaded via -map=) and a
single AMidanTrackSpline placed in it. Exits non-zero if no track spline is
found or generation fails, so CI can gate on it after a track layout change.

This script deliberately contains NO placement logic. Even spacing and
sector bucketing live in C++ (UMidanCheckpointGeneratorLibrary), which wraps
math in MidanRace that an Automation Spec also exercises — see
docs/MANUAL_STEPS.md Phase 5. Duplicating the arithmetic here would let the
two drift.
"""

import sys
import unreal


DEFAULT_CHECKPOINT_COUNT = 24
DEFAULT_SECTOR_COUNT = 3


def find_track_spline():
    """Return the single AMidanTrackSpline in the currently open level."""
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = subsystem.get_all_level_actors()

    splines = [a for a in all_actors if isinstance(a, unreal.MidanTrackSpline)]

    if not splines:
        return None
    if len(splines) > 1:
        unreal.log_warning(
            f"Found {len(splines)} AMidanTrackSpline actors; using the first "
            f"({splines[0].get_name()}). A level should have exactly one."
        )
    return splines[0]


def main():
    unreal.log("")
    unreal.log("MIDAN - CHECKPOINT GENERATION")
    unreal.log("")

    checkpoint_count = DEFAULT_CHECKPOINT_COUNT
    sector_count = DEFAULT_SECTOR_COUNT

    for arg in sys.argv:
        if arg.startswith("-checkpoints="):
            checkpoint_count = int(arg.split("=", 1)[1])
        elif arg.startswith("-sectors="):
            sector_count = int(arg.split("=", 1)[1])

    track = find_track_spline()
    if track is None:
        unreal.log_error(
            "No AMidanTrackSpline found in the open level. Open the track map "
            "and place one before running this script."
        )
        return 1

    unreal.log(f"Track: {track.get_name()}  Checkpoints: {checkpoint_count}  Sectors: {sector_count}")

    checkpoints = unreal.Array(unreal.MidanCheckpoint)
    success = unreal.MidanCheckpointGeneratorLibrary.generate_checkpoints(
        track, checkpoint_count, sector_count, checkpoints
    )

    if not success:
        unreal.log_error("Checkpoint generation failed. See the Output Log above for the reason.")
        return 1

    unreal.log(f"Generated {len(checkpoints)} checkpoints.")

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(save_map_packages=True, save_content_packages=False)

    return 0


if __name__ == "__main__":
    sys.exit(main())
