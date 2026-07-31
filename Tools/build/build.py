#!/usr/bin/env python3
"""Wraps UAT BuildCookRun to produce a Midan build.

Usage:

    python3 Tools/build/build.py --platform Mac --configuration Test
    python3 Tools/build/build.py --platform Win64 --configuration Shipping --archive-dir Builds/Win64

--platform Mac|Win64 — Mac-first per docs/ASSUMPTIONS.md A4; the Win64 lane
is gated on runner availability (docs/PHASE_PLAN.md Phase 10, .github/workflows/ci.yml).

--configuration is passed straight through to UAT — Development, Test, or
Shipping. CLAUDE.md's measurement rule requires Test (never Development) for
any build a performance number is reported from; this script does not
enforce that itself (a Development build is a legitimate thing to produce),
but Tools/analysis/perf_report.py's --config flag is where that rule is
actually checked.

This script does not invent the UE install path — pass --engine-dir, or set
the MIDAN_ENGINE_DIR environment variable, or it looks for the conventional
"/Users/Shared/Epic Games/UE_5.8" (Mac) location docs/MANUAL_STEPS.md uses
throughout.
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path


DEFAULT_MAC_ENGINE_DIR = "/Users/Shared/Epic Games/UE_5.8"
PROJECT_NAME = "Midan"


def find_engine_dir(args):
    if args.engine_dir:
        return Path(args.engine_dir)
    env_dir = os.environ.get("MIDAN_ENGINE_DIR")
    if env_dir:
        return Path(env_dir)
    if sys.platform == "darwin":
        return Path(DEFAULT_MAC_ENGINE_DIR)
    print("ERROR: --engine-dir or MIDAN_ENGINE_DIR is required on this platform "
          "(no conventional default known for it).", file=sys.stderr)
    sys.exit(1)


def find_uat_script(engine_dir):
    if sys.platform == "darwin" or sys.platform.startswith("linux"):
        return engine_dir / "Engine" / "Build" / "BatchFiles" / "RunUAT.sh"
    return engine_dir / "Engine" / "Build" / "BatchFiles" / "RunUAT.bat"


def project_root():
    return Path(__file__).resolve().parents[2]


def build(args):
    engine_dir = find_engine_dir(args)
    uat_script = find_uat_script(engine_dir)

    if not uat_script.is_file():
        print(f"ERROR: RunUAT not found at {uat_script}. Check --engine-dir / MIDAN_ENGINE_DIR.", file=sys.stderr)
        return 1

    uproject_path = project_root() / f"{PROJECT_NAME}.uproject"
    if not uproject_path.is_file():
        print(f"ERROR: {uproject_path} not found. Run this from the repo, or check project_root().", file=sys.stderr)
        return 1

    archive_dir = Path(args.archive_dir) if args.archive_dir else (project_root() / "Builds" / args.platform)
    archive_dir.mkdir(parents=True, exist_ok=True)

    # API VERIFY: BuildCookRun's exact flag set is unconfirmed without an
    # installed engine (docs/ASSUMPTIONS.md) — this is the standard shape per
    # Epic's documented BuildCookRun usage, not verified against 5.8.
    cmd = [
        str(uat_script),
        "BuildCookRun",
        f"-project={uproject_path}",
        "-noP4",
        f"-platform={args.platform}",
        f"-clientconfig={args.configuration}",
        f"-serverconfig={args.configuration}",
        "-build",
        "-cook",
        "-stage",
        "-pak",
        "-archive",
        f"-archivedirectory={archive_dir}",
    ]

    if args.configuration == "Shipping":
        cmd.append("-distribution")

    print(f"Running: {' '.join(cmd)}")

    if args.dry_run:
        print("(--dry-run: not executing)")
        return 0

    result = subprocess.run(cmd, cwd=str(project_root()))
    if result.returncode != 0:
        print(f"ERROR: BuildCookRun exited {result.returncode}.", file=sys.stderr)
        return result.returncode

    print(f"Build archived to {archive_dir}")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--platform", required=True, choices=["Mac", "Win64"], help="Target platform.")
    parser.add_argument("--configuration", default="Development", choices=["Development", "Test", "Shipping"])
    parser.add_argument("--engine-dir", default=None, help="Path to the UE 5.8 install. Falls back to MIDAN_ENGINE_DIR, then a Mac default.")
    parser.add_argument("--archive-dir", default=None, help="Output directory. Defaults to Builds/<platform>.")
    parser.add_argument("--dry-run", action="store_true", help="Print the command without running it.")
    args = parser.parse_args()

    return build(args)


if __name__ == "__main__":
    sys.exit(main())
