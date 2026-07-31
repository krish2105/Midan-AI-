#!/usr/bin/env python3
"""Thin wrapper over itch.io's `butler` CLI to push a verified build.

Usage:

    python3 Tools/build/upload_itch.py --platform Mac --build-dir Builds/Mac \\
        --project <user-or-org>/midan --channel mac-test

Deliberately refuses to run unless Tools/build/verify_build.py has already
passed against the same build-dir in this invocation — uploading a build
that failed verification defeats the entire point of having gates. Pass
--skip-verify only for a genuinely non-shipping internal test channel, and
never for a channel a reviewer might see.

Requires `butler` on PATH (https://itch.io/docs/butler/) and
`butler login` already run once, interactively, outside this script —
this script never touches or asks for credentials itself.
"""

import argparse
import subprocess
import sys
from pathlib import Path


def run_verify(build_dir: str, platform: str) -> bool:
    tools_dir = Path(__file__).resolve().parent
    cmd = [sys.executable, str(tools_dir / "verify_build.py"), "--platform", platform, "--build-dir", build_dir]
    print(f"Running verification first: {' '.join(cmd)}")
    return subprocess.run(cmd).returncode == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--platform", required=True, choices=["Mac", "Win64"])
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--project", required=True, help="itch.io project, e.g. yourname/midan.")
    parser.add_argument("--channel", required=True, help="itch.io channel, e.g. mac-test, win-test, mac-release.")
    parser.add_argument("--skip-verify", action="store_true", help="Only for an internal, non-public test channel. See the module docstring.")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if not args.skip_verify:
        if not run_verify(args.build_dir, args.platform):
            print("ERROR: verification failed — not uploading. Pass --skip-verify only for an internal test channel.", file=sys.stderr)
            return 1
    else:
        print("WARNING: --skip-verify set. This build has NOT been confirmed to pass the hard gates.", file=sys.stderr)

    target = f"{args.project}:{args.channel}"
    cmd = ["butler", "push", args.build_dir, target]

    print(f"$ {' '.join(cmd)}")
    if args.dry_run:
        print("(--dry-run: not executing)")
        return 0

    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f"ERROR: butler push exited {result.returncode}. Is 'butler login' set up?", file=sys.stderr)
        return result.returncode

    print(f"Uploaded {args.build_dir} to {target}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
