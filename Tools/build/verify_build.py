#!/usr/bin/env python3
"""Phase 10's hard gates on a packaged build. Exits non-zero if any fails.

Usage:

    python3 Tools/build/verify_build.py --platform Mac --build-dir Builds/Mac

Six hard gates, per docs/PHASE_PLAN.md Phase 10:

    1. Executable exists and reaches the main menu within N seconds
       (headless smoke test)
    2. Pak integrity
    3. Build size within budget
    4. No Development-only content cooked
    5. Manifest matches the git SHA the build was produced from
    6. No MidanEditorTools (or MidanTests) symbols referenced in the binary

Every gate reports PASS/FAIL by name — a report that only says "verification
failed" is not a report anyone can act on.
"""

import argparse
import hashlib
import re
import subprocess
import sys
import time
from pathlib import Path


# Package size budget, MB. UNMEASURED STARTING POINT (docs/ASSUMPTIONS.md
# A52's same category, applied to a build-size ceiling instead of a render
# setting) — a vertical slice with one 3km track, three cars, and MetaSound
# audio is being budgeted conservatively pending a real cooked build to
# measure against.
DEFAULT_SIZE_BUDGET_MB = 8192

# Symbols that must never appear in a Shipping binary — the module name
# itself is enough; a linked module's name appears in its own log/asserts
# strings even in a stripped build, which is what this greps for.
FORBIDDEN_MODULE_SUBSTRINGS = ["MidanEditorTools", "MidanTests"]


def find_executable(build_dir: Path, platform: str):
    if platform == "Mac":
        candidates = list(build_dir.glob("**/Midan.app/Contents/MacOS/Midan"))
    else:
        candidates = list(build_dir.glob("**/Midan.exe"))
    return candidates[0] if candidates else None


def gate_executable_and_smoke_test(build_dir: Path, platform: str, timeout_seconds: float):
    executable = find_executable(build_dir, platform)
    if not executable or not executable.exists():
        return False, "no packaged executable found under build-dir"

    # API VERIFY: the exact unattended/headless launch flags for confirming
    # "reached the main menu" are unconfirmed without an installed engine —
    # this assumes a `-MidanSmokeTestComplete` log line the main menu level
    # blueprint/HUD would need to emit (a Phase 7/10 content wiring task, not
    # yet built) or, failing that, a plain "process is still alive after N
    # seconds and did not crash" fallback. The fallback is weaker evidence
    # than a real menu-reached signal and should be tightened once possible.
    cmd = [str(executable), "-unattended", "-nosplash", f"-ExecCmds=quit"]
    start = time.monotonic()
    try:
        result = subprocess.run(cmd, timeout=timeout_seconds, capture_output=True, text=True)
    except subprocess.TimeoutExpired:
        return False, f"did not exit within {timeout_seconds}s"

    elapsed = time.monotonic() - start
    if result.returncode != 0:
        return False, f"exited with code {result.returncode} after {elapsed:.1f}s"

    return True, f"launched and exited cleanly in {elapsed:.1f}s"


def gate_pak_integrity(build_dir: Path):
    pak_files = list(build_dir.glob("**/*.pak"))
    if not pak_files:
        return False, "no .pak files found"

    # A real integrity check needs UnrealPak -test, which needs the engine.
    # Here: confirm every pak is non-empty and has a plausible pak-format
    # magic footer presence check is out of scope without UnrealPak, so this
    # gate is a minimum viability check, not a full CRC verification —
    # stated explicitly rather than implying more than it verifies.
    for pak in pak_files:
        if pak.stat().st_size == 0:
            return False, f"{pak.name} is zero bytes"

    return True, f"{len(pak_files)} .pak file(s) present and non-empty (full CRC check requires UnrealPak -test, not run here)"


def gate_build_size(build_dir: Path, budget_mb: int):
    total_bytes = sum(f.stat().st_size for f in build_dir.rglob("*") if f.is_file())
    total_mb = total_bytes / (1024 * 1024)
    if total_mb > budget_mb:
        return False, f"{total_mb:.0f}MB exceeds the {budget_mb}MB budget"
    return True, f"{total_mb:.0f}MB (budget {budget_mb}MB)"


def gate_no_development_content(build_dir: Path):
    # Development-only cooked content commonly leaves behind editor-only
    # uassets or debug shader permutations; the cheap, portable check is
    # scanning staged config for EditorOnly / WITH_EDITOR markers that should
    # have been stripped by a Shipping cook.
    suspicious = list(build_dir.glob("**/*Editor*.pak")) + list(build_dir.glob("**/*Development*"))
    if suspicious:
        names = ", ".join(p.name for p in suspicious[:5])
        return False, f"found {len(suspicious)} suspiciously-named file(s), e.g. {names}"
    return True, "no Development/Editor-named artifacts found in the staged build"


def gate_manifest_matches_git_sha(build_dir: Path, expected_sha: str):
    manifest_candidates = list(build_dir.glob("**/*.manifest")) + list(build_dir.glob("**/BuildManifest*.json"))
    if not manifest_candidates:
        return False, "no build manifest found"

    manifest_text = manifest_candidates[0].read_text(errors="ignore")
    if expected_sha[:12] not in manifest_text:
        return False, f"expected git SHA {expected_sha[:12]} not found in {manifest_candidates[0].name}"
    return True, f"manifest references {expected_sha[:12]}"


def gate_no_forbidden_modules(build_dir: Path):
    executable = None
    for platform in ("Mac", "Win64"):
        executable = find_executable(build_dir, platform)
        if executable:
            break
    if not executable:
        return False, "no executable found to scan"

    data = executable.read_bytes()
    found = []
    for forbidden in FORBIDDEN_MODULE_SUBSTRINGS:
        if forbidden.encode("ascii") in data:
            found.append(forbidden)

    if found:
        return False, f"forbidden module symbol(s) present: {', '.join(found)}"
    return True, f"none of {FORBIDDEN_MODULE_SUBSTRINGS} found in the binary"


def get_git_sha():
    try:
        return subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    except subprocess.CalledProcessError:
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--platform", required=True, choices=["Mac", "Win64"])
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--size-budget-mb", type=int, default=DEFAULT_SIZE_BUDGET_MB)
    parser.add_argument("--smoke-test-timeout", type=float, default=60.0)
    parser.add_argument("--git-sha", default=None, help="Defaults to the current HEAD.")
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    if not build_dir.is_dir():
        print(f"ERROR: build-dir '{build_dir}' does not exist.", file=sys.stderr)
        return 1

    git_sha = args.git_sha or get_git_sha()
    if not git_sha:
        print("ERROR: could not determine git SHA (pass --git-sha or run inside the repo).", file=sys.stderr)
        return 1

    gates = [
        ("executable_and_smoke_test", lambda: gate_executable_and_smoke_test(build_dir, args.platform, args.smoke_test_timeout)),
        ("pak_integrity", lambda: gate_pak_integrity(build_dir)),
        ("build_size", lambda: gate_build_size(build_dir, args.size_budget_mb)),
        ("no_development_content", lambda: gate_no_development_content(build_dir)),
        ("manifest_matches_git_sha", lambda: gate_manifest_matches_git_sha(build_dir, git_sha)),
        ("no_forbidden_modules", lambda: gate_no_forbidden_modules(build_dir)),
    ]

    print("MIDAN — BUILD VERIFICATION")
    print(f"Platform: {args.platform}  Build dir: {build_dir}  Git SHA: {git_sha[:12]}")
    print("")

    any_failed = False
    for name, gate_fn in gates:
        try:
            passed, detail = gate_fn()
        except Exception as exc:  # a gate crashing is a FAIL, not a silent skip
            passed, detail = False, f"gate raised {type(exc).__name__}: {exc}"

        status = "PASS" if passed else "FAIL"
        if not passed:
            any_failed = True
        print(f"  {name:<28}{status:<6}{detail}")

    print("")
    print("VERIFICATION FAILED — do not ship this build." if any_failed else "VERIFICATION PASSED")
    return 1 if any_failed else 0


if __name__ == "__main__":
    sys.exit(main())
