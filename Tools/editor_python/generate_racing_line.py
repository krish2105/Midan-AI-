"""Generate the AI racing-line speed profile from the editor.

Run inside the editor (Output Log -> Python), or headless:

    <UE>/Engine/Binaries/Mac/UnrealEditor-Cmd \\
        <abs>/Midan.uproject \\
        -run=pythonscript -script="<abs>/Tools/editor_python/generate_racing_line.py"

Requires the map with the track already open and a single AMidanRacingLineSpline
placed in it, with Points already resized to match the spline's key count
(add/remove FRacingLinePoint entries by hand until Points.Num() equals the
spline's point count — see docs/MANUAL_STEPS.md Phase 6). Exits non-zero if no
racing line is found or generation fails.

This script deliberately contains NO speed-profile math. The forward
cornering-limit pass and the backward braking pass both live in C++
(MidanSpeedProfileGenerator), which an Automation Spec also exercises against
a synthetic corner sequence — see docs/MANUAL_STEPS.md Phase 6. Duplicating
the arithmetic here would let the two drift.
"""

import sys
import unreal


DEFAULT_MU_EFFECTIVE = 1.0
DEFAULT_VMAX_KMH = 320.0
DEFAULT_MAX_BRAKING_DECEL_CMS2 = 1400.0


def find_racing_line():
    """Return the single AMidanRacingLineSpline in the currently open level."""
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = subsystem.get_all_level_actors()

    lines = [a for a in all_actors if isinstance(a, unreal.MidanRacingLineSpline)]

    if not lines:
        return None
    if len(lines) > 1:
        unreal.log_warning(
            f"Found {len(lines)} AMidanRacingLineSpline actors; using the first "
            f"({lines[0].get_name()}). A level should have exactly one."
        )
    return lines[0]


def main():
    unreal.log("")
    unreal.log("MIDAN - RACING LINE SPEED PROFILE GENERATION")
    unreal.log("")

    mu_effective = DEFAULT_MU_EFFECTIVE
    vmax_kmh = DEFAULT_VMAX_KMH
    max_braking_decel = DEFAULT_MAX_BRAKING_DECEL_CMS2

    for arg in sys.argv:
        if arg.startswith("-mu="):
            mu_effective = float(arg.split("=", 1)[1])
        elif arg.startswith("-vmax="):
            vmax_kmh = float(arg.split("=", 1)[1])
        elif arg.startswith("-decel="):
            max_braking_decel = float(arg.split("=", 1)[1])

    racing_line = find_racing_line()
    if racing_line is None:
        unreal.log_error(
            "No AMidanRacingLineSpline found in the open level. Place one and "
            "author Points (one per spline key, at minimum LateralOffsetMinCm/"
            "MaxCm) before running this script."
        )
        return 1

    unreal.log(
        f"RacingLine: {racing_line.get_name()}  mu={mu_effective}  "
        f"VMax={vmax_kmh}km/h  MaxBrakingDecel={max_braking_decel}cm/s^2"
    )

    success = unreal.MidanRacingLineToolLibrary.generate_racing_line_speed_profile(
        racing_line, mu_effective, vmax_kmh, max_braking_decel
    )

    if not success:
        unreal.log_error("Speed profile generation failed. See the Output Log above for the reason.")
        return 1

    unreal.log("Speed profile generated and applied.")

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(save_map_packages=True, save_content_packages=False)

    return 0


if __name__ == "__main__":
    sys.exit(main())
