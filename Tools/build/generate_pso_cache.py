#!/usr/bin/env python3
"""Collect and expand the PSO cache: one collection playthrough per lighting
preset (Day, Night, Deep Night — docs/ART_DIRECTION.md v2.0's three static
presets), unioned into the cache shipped in the build.

Usage:

    python3 Tools/build/generate_pso_cache.py --platform Mac --build-dir Builds/Mac

Master prompt §7.3's collection step, done three times: docs/ASSUMPTIONS.md
A22/A23 record that the pivot to three time-of-day presets triples the PSO
cache collection surface — "the shipped cache is the union [of three runs];
skipping one ships a build that hitches on first selection of that preset."
This script enforces that by running all three unless told otherwise.

The before/after hitch-count numbers this produces are the ones
docs/PERFORMANCE_BUDGET.md §3.3 and the README's performance table both
require — CLAUDE.md's measurement rule applies to them exactly as it does to
frame time: unmeasured means it does not go in either document.

Windows-oriented toolchain (docs/ASSUMPTIONS.md A19): `ShaderPipelineCacheTools`
Expand is documented primarily for the Win64 pipeline. On Mac this script
still runs the collection playthroughs and records raw .rec.upipelinecache
files; the Expand/merge step is marked and may need the Win64 lane to
complete, in which case that gap is stated in the README rather than
papered over with an invented number.
"""

import argparse
import subprocess
import sys
from pathlib import Path


PRESETS = ["Day", "Night", "DeepNight"]


def find_executable(build_dir: Path, platform: str) -> Path:
    if platform == "Mac":
        candidates = list(build_dir.glob("**/Midan.app/Contents/MacOS/Midan"))
    else:
        candidates = list(build_dir.glob("**/Midan.exe"))
    return candidates[0] if candidates else None


def run_collection_playthrough(executable: Path, preset: str, output_dir: Path, dry_run: bool):
    output_dir.mkdir(parents=True, exist_ok=True)
    cache_file = output_dir / f"pso_collect_{preset}.rec.upipelinecache"

    # API VERIFY: exact PSO-collection launch args are unconfirmed without an
    # installed engine — the shape below follows Epic's documented
    # -PSOCollection / stat pso workflow. docs/MANUAL_STEPS.md Phase 10 has
    # the human-in-the-loop steps (drive the hot lap under each preset) this
    # script assumes are scripted via the same deterministic hot-lap replay
    # Phase 8 uses for profiling.
    cmd = [
        str(executable),
        f"-ExecCmds=r.ShaderPipelineCache.Enabled 1, r.ShaderPipelineCache.LogPSO 1",
        f"-MidanLightingPreset={preset}",
        f"-PSOCollectionFile={cache_file}",
        "-unattended",
        "-nosplash",
    ]

    print(f"$ {' '.join(cmd)}")
    if dry_run:
        return cache_file

    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f"WARNING: collection run for preset '{preset}' exited {result.returncode}.", file=sys.stderr)

    return cache_file


def expand_cache(collected_files, output_path: Path, dry_run: bool) -> bool:
    # API VERIFY (docs/ASSUMPTIONS.md A19): ShaderPipelineCacheTools is a
    # Windows-oriented toolchain path. This call may need to run on the Win64
    # lane rather than here — if it fails on Mac for that reason, the raw
    # per-preset .rec.upipelinecache files above are still produced and
    # usable once a Windows host expands them.
    cmd = ["ShaderPipelineCacheTools", "Expand"] + [str(f) for f in collected_files] + [str(output_path)]
    print(f"$ {' '.join(cmd)}")
    if dry_run:
        return True

    result = subprocess.run(cmd)
    return result.returncode == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--platform", required=True, choices=["Mac", "Win64"])
    parser.add_argument("--build-dir", required=True, help="Directory produced by build.py --configuration Shipping.")
    parser.add_argument("--presets", nargs="+", default=PRESETS, help="Lighting presets to collect. Defaults to all three — see the module docstring for why skipping one is a real gap, not a shortcut.")
    parser.add_argument("--out", default=None, help="Merged cache output path. Defaults to <build-dir>/pso_cache_merged.upipelinecache.")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    executable = find_executable(build_dir, args.platform)
    if not executable and not args.dry_run:
        print(f"ERROR: could not find the packaged executable under {build_dir}.", file=sys.stderr)
        return 1

    output_dir = build_dir / "PSOCollection"
    collected_files = []
    for preset in args.presets:
        collected_files.append(run_collection_playthrough(executable or Path("<dry-run>"), preset, output_dir, args.dry_run))

    if len(args.presets) < 3:
        print(f"WARNING: only {len(args.presets)}/3 presets collected. The shipped cache will hitch on first "
              f"selection of any preset not in {args.presets}. See docs/ASSUMPTIONS.md A23.", file=sys.stderr)

    merged_path = Path(args.out) if args.out else (build_dir / "pso_cache_merged.upipelinecache")
    if not expand_cache(collected_files, merged_path, args.dry_run):
        print("ERROR: ShaderPipelineCacheTools Expand failed. See the Windows-toolchain note in this "
              "script's docstring — the per-preset collection files are still usable directly.", file=sys.stderr)
        return 1

    print(f"PSO cache merged to {merged_path}")
    print("Next: measure hitch count with and without this cache bundled, per docs/PERFORMANCE_BUDGET.md SS3.3 — "
          "both numbers, never just the improved one, per the measurement rule.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
