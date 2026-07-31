#!/usr/bin/env python3
"""Produce a Shipping build via build.py, then run the Phase 10 gate chain:
PSO cache, verification.

Usage:

    python3 Tools/build/package_shipping.py --platform Mac

This is the "one command" the master prompt's acceptance criterion asks for
(docs/ASSUMPTIONS.md A4) — on Mac, today; --platform Win64 works once a
Windows host or runner exists, same script.

Deliberately thin: each step is its own script (build.py, generate_pso_cache.py,
verify_build.py) so a failure at step N is diagnosable by re-running that one
script directly, rather than needing to re-run the whole chain to reproduce it.
"""

import argparse
import subprocess
import sys
from pathlib import Path


def run_step(description, cmd):
    print(f"\n=== {description} ===")
    print(f"$ {' '.join(cmd)}")
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f"ERROR: '{description}' failed (exit {result.returncode}). Stopping — "
              f"see docs/PHASE_PLAN.md Phase 10: do not proceed past a failed step.", file=sys.stderr)
        return False
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--platform", required=True, choices=["Mac", "Win64"])
    parser.add_argument("--engine-dir", default=None)
    parser.add_argument("--archive-dir", default=None)
    parser.add_argument("--skip-pso-cache", action="store_true", help="Skip PSO cache generation — for a quick iteration build, never for a real ship candidate.")
    parser.add_argument("--skip-verify", action="store_true", help="Skip Tools/build/verify_build.py — same caveat as --skip-pso-cache.")
    args = parser.parse_args()

    tools_dir = Path(__file__).resolve().parent

    build_cmd = [sys.executable, str(tools_dir / "build.py"), "--platform", args.platform, "--configuration", "Shipping"]
    if args.engine_dir:
        build_cmd += ["--engine-dir", args.engine_dir]
    if args.archive_dir:
        build_cmd += ["--archive-dir", args.archive_dir]

    if not run_step("Build (Shipping)", build_cmd):
        return 1

    archive_dir = args.archive_dir or str(Path.cwd() / "Builds" / args.platform)

    if not args.skip_pso_cache:
        pso_cmd = [sys.executable, str(tools_dir / "generate_pso_cache.py"), "--platform", args.platform, "--build-dir", archive_dir]
        if not run_step("Generate PSO cache", pso_cmd):
            return 1

    if not args.skip_verify:
        verify_cmd = [sys.executable, str(tools_dir / "verify_build.py"), "--platform", args.platform, "--build-dir", archive_dir]
        if not run_step("Verify build (hard gates)", verify_cmd):
            return 1

    print(f"\nShipping build ready at {archive_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
