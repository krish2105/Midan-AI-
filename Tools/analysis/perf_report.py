#!/usr/bin/env python3
"""Emit the docs/PERFORMANCE_BUDGET.md §8 report from one or more UE CSV
Profiler exports (one CSV per deterministic hot-lap run).

Usage:

    python3 Tools/analysis/perf_report.py \\
        --csv Saved/Profiling/run1.csv --csv Saved/Profiling/run2.csv --csv Saved/Profiling/run3.csv \\
        --config Test --resolution "1080p @ TSR 67%" \\
        --machine "Apple M4 Pro / macOS 15.2" --tier Epic \\
        --physics-allocations 0 --telemetry-gamethread-io 0 --editor-module-in-shipping 0

Each --csv is one run of the SAME hot-lap replay
(AMidanHotLapReplay, Phase 8) — docs/PERFORMANCE_BUDGET.md §4 requires a
minimum of 3 for the p95/max-delta provenance field to mean anything, and
this script refuses fewer than 3 unless --allow-fewer-runs is passed
explicitly (so a rushed 1-run check cannot masquerade as a real gate result).

The three hard gates this script cannot derive from a CSV Profiler export —
physics-callback allocations, telemetry game-thread I/O, and
MidanEditorTools symbols in a Shipping binary — are supplied as explicit CLI
flags rather than guessed. Per CLAUDE.md's measurement rule, an unverified
gate does not silently pass: omitting a flag marks that line UNVERIFIED, not
PASS, and UNVERIFIED on a hard gate fails the whole report.

Deliberately stdlib-only — same reasoning as telemetry_report.py.
"""

import argparse
import csv
import os
import sys


# --- Budget, from docs/PERFORMANCE_BUDGET.md §1, §2, §3.1. Keep in sync with
# that document by hand; it is the single source of truth for the numbers,
# this is a second, small, human-checked copy of them for the gate logic. ---

HARD_GATES = {
    "frame_time_p95_ms": ("<=", 16.6),
    "draw_calls_peak": ("<=", 3000),
    "physics_callback_allocations": ("==", 0),
    "telemetry_gamethread_io": ("==", 0),
    "editor_module_in_shipping": ("==", 0),
}

GPU_TUNING_LINES = {
    "base_pass_nanite_ms": ("<=", 3.0),
    "lumen_ms": ("<=", 4.0),
    "virtual_shadow_maps_ms": ("<=", 1.5),
    "volumetric_ms": ("<=", 1.0),
    "post_tsr_ms": ("<=", 2.5),
    "translucency_ms": ("<=", 1.25),
    "gpu_total_ms": ("<=", 15.0),
}

CPU_TUNING_LINES = {
    "game_thread_ms": ("<=", 6.0),
    "render_thread_ms": ("<=", 6.0),
    "rhi_thread_ms": ("<=", 4.0),
}

# CSV Profiler column name candidates per metric, checked case-insensitively
# in order — UE's exact stat names vary by version and which stat groups
# were enabled during capture (-csvGpuStats etc.). API VERIFY: unconfirmed
# against a real 5.8 CSV Profiler export; adjust this list once one exists.
COLUMN_CANDIDATES = {
    "frame_time_p95_ms": ["FrameTime", "Frame"],
    "draw_calls_peak": ["DrawCalls", "RHI/DrawCalls"],
    "base_pass_nanite_ms": ["BasePass", "Nanite/BasePass", "GPU/BasePass"],
    "lumen_ms": ["Lumen", "GPU/Lumen", "LumenSceneUpdate"],
    "virtual_shadow_maps_ms": ["ShadowDepths", "VSM", "GPU/ShadowDepths"],
    "volumetric_ms": ["VolumetricFog", "GPU/VolumetricFog"],
    "post_tsr_ms": ["PostProcessing", "TSR", "GPU/PostProcessing"],
    "translucency_ms": ["Translucency", "GPU/Translucency"],
    "gpu_total_ms": ["GPUTime", "GPU/Total"],
    "game_thread_ms": ["GameThreadTime", "FrameTime_GameThread"],
    "render_thread_ms": ["RenderThreadTime", "FrameTime_RenderThread"],
    "rhi_thread_ms": ["RHIThreadTime", "FrameTime_RHIThread"],
}


def percentile(values, pct):
    if not values:
        return None
    values_sorted = sorted(values)
    index = min(int(len(values_sorted) * pct), len(values_sorted) - 1)
    return values_sorted[index]


def find_column(header, candidates):
    lower_header = {h.lower(): h for h in header}
    for candidate in candidates:
        if candidate.lower() in lower_header:
            return lower_header[candidate.lower()]
    return None


def load_run(csv_path):
    """Returns {metric_name: [per-frame float values]} for one run's CSV."""
    if not os.path.isfile(csv_path):
        print(f"ERROR: CSV file not found: {csv_path}", file=sys.stderr)
        sys.exit(1)

    with open(csv_path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        header = reader.fieldnames or []
        columns = {metric: find_column(header, candidates) for metric, candidates in COLUMN_CANDIDATES.items()}

        series = {metric: [] for metric in COLUMN_CANDIDATES}
        for row in reader:
            for metric, column in columns.items():
                if not column:
                    continue
                try:
                    series[metric].append(float(row[column]))
                except (ValueError, KeyError):
                    continue

    missing = [metric for metric, column in columns.items() if not column]
    return series, missing


def aggregate_runs(csv_paths):
    """Returns (values, missing_columns_by_run, per_run_frame_p95)."""
    all_series = {metric: [] for metric in COLUMN_CANDIDATES}
    missing_by_run = {}
    per_run_frame_p95 = []

    for path in csv_paths:
        series, missing = load_run(path)
        missing_by_run[path] = missing

        frame_values = series.get("frame_time_p95_ms", [])
        p95 = percentile(frame_values, 0.95)
        if p95 is not None:
            per_run_frame_p95.append(p95)

        for metric, values in series.items():
            all_series[metric].extend(values)

    results = {}
    # Frame time p95: worst (max) of the per-run p95s, not pooled across
    # runs — a single bad run must not be smoothed away by averaging with
    # two good ones.
    results["frame_time_p95_ms"] = max(per_run_frame_p95) if per_run_frame_p95 else None
    results["draw_calls_peak"] = max(all_series["draw_calls_peak"]) if all_series["draw_calls_peak"] else None

    for metric in list(GPU_TUNING_LINES.keys()) + list(CPU_TUNING_LINES.keys()):
        values = all_series.get(metric, [])
        # Worst-case run average for tuning lines: mean per run, then the
        # max of those means — consistent with the hard-gate line above.
        results[metric] = max(values) if values else None

    run_to_run_max_delta_ms = (max(per_run_frame_p95) - min(per_run_frame_p95)) if len(per_run_frame_p95) > 1 else 0.0

    return results, missing_by_run, run_to_run_max_delta_ms


def evaluate(name, value, rule):
    op, target = rule
    if value is None:
        return "UNVERIFIED"
    if op == "<=":
        return "PASS" if value <= target else "FAIL"
    if op == "==":
        return "PASS" if value == target else "FAIL"
    return "UNVERIFIED"


def fmt(value, decimals=1):
    return "----" if value is None else f"{value:.{decimals}f}"


def build_report(args, results, run_to_run_max_delta_ms, missing_by_run):
    lines = []
    lines.append("MIDAN — FRAME BUDGET REPORT")
    lines.append(f"Config: {args.config} · {args.resolution} · {args.machine} · Tier: {args.tier}")
    lines.append(f"Hot-lap replay · {len(args.csv)} run(s) · reporting p95")
    lines.append("")

    hard_gate_values = dict(results)
    hard_gate_values["physics_callback_allocations"] = args.physics_allocations
    hard_gate_values["telemetry_gamethread_io"] = args.telemetry_gamethread_io
    hard_gate_values["editor_module_in_shipping"] = args.editor_module_in_shipping

    lines.append("HARD GATES")
    any_hard_failure = False
    failing_lines = []
    for name, rule in HARD_GATES.items():
        value = hard_gate_values.get(name)
        verdict = evaluate(name, value, rule)
        if verdict != "PASS":
            any_hard_failure = True
            failing_lines.append(name)
        op, target = rule
        lines.append(f"  {name:<32}{fmt(value, 0 if op == '==' else 1):>8}    [{op}{target}]{'':<6}{verdict}")
    lines.append("")

    lines.append("TUNING (GPU)")
    for name, rule in GPU_TUNING_LINES.items():
        value = results.get(name)
        verdict = evaluate(name, value, rule)
        if verdict == "FAIL":
            failing_lines.append(name)
        op, target = rule
        lines.append(f"  {name:<32}{fmt(value):>8}    [{op}{target}]{'':<6}{verdict}")
    lines.append("")

    lines.append("TUNING (CPU)")
    for name, rule in CPU_TUNING_LINES.items():
        value = results.get(name)
        verdict = evaluate(name, value, rule)
        if verdict == "FAIL":
            failing_lines.append(name)
        op, target = rule
        lines.append(f"  {name:<32}{fmt(value):>8}    [{op}{target}]{'':<6}{verdict}")
    lines.append("")

    lines.append("OPERATING")
    lines.append(f"  {'package_size_mb':<32}{'----':>8}    [budget TBD]  ----   (not derivable from a CSV Profiler run — see Tools/build/verify_build.py, Phase 10)")
    lines.append(f"  {'cold_launch_seconds':<32}{'----':>8}    [<=N]         ----   (same)")
    lines.append(f"  {'hitch_count_pre_pso':<32}{'----':>8}    [baseline]    ----   (Phase 10)")
    lines.append(f"  {'hitch_count_post_pso':<32}{'----':>8}    [< pre]       ----   (Phase 10)")
    lines.append(f"  {'run_to_run_max_delta_ms':<32}{run_to_run_max_delta_ms:>8.1f}    [<=1.0]       {'PASS' if run_to_run_max_delta_ms <= 1.0 else 'FAIL'}")
    lines.append("")

    lines.append(f"Failures: {', '.join(failing_lines) if failing_lines else 'none'}")

    missing_note = []
    for path, missing in missing_by_run.items():
        if missing:
            missing_note.append(f"{path}: could not find columns for {', '.join(missing)}")
    if missing_note:
        lines.append("Known limitations: " + "; ".join(missing_note))
    else:
        lines.append("Known limitations: none stated")

    return "\n".join(lines), any_hard_failure


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--csv", action="append", required=True, help="Path to one run's CSV Profiler export. Repeat for multiple runs.")
    parser.add_argument("--config", required=True, help='Build configuration, e.g. "Test". Never "Development" — see the measurement rule.')
    parser.add_argument("--resolution", required=True, help='e.g. "1080p @ TSR 67%%"')
    parser.add_argument("--machine", required=True, help='e.g. "Apple M4 Pro / macOS 15.2"')
    parser.add_argument("--tier", required=True, help="Scalability tier active during capture, e.g. Epic.")
    parser.add_argument("--physics-allocations", type=int, default=None, help="Heap allocations observed inside any physics callback. Required for the hard gate to read PASS rather than UNVERIFIED.")
    parser.add_argument("--telemetry-gamethread-io", type=int, default=None, help="Game-thread file I/O events observed during telemetry capture.")
    parser.add_argument("--editor-module-in-shipping", type=int, default=None, help="1 if MidanEditorTools symbols were found in a Shipping binary, else 0.")
    parser.add_argument("--allow-fewer-runs", action="store_true", help="Bypass the minimum-3-runs check (docs/PERFORMANCE_BUDGET.md SS4). Only for iterating quickly outside a real gate.")
    parser.add_argument("--out", default=None, help="Write the report to this path in addition to stdout.")
    args = parser.parse_args()

    if len(args.csv) < 3 and not args.allow_fewer_runs:
        print(f"ERROR: {len(args.csv)} run(s) supplied; docs/PERFORMANCE_BUDGET.md SS4 requires a minimum of 3. "
              f"Pass --allow-fewer-runs to override for a quick local check (not a real gate run).", file=sys.stderr)
        return 1

    results, missing_by_run, run_to_run_max_delta_ms = aggregate_runs(args.csv)
    report, any_hard_failure = build_report(args, results, run_to_run_max_delta_ms, missing_by_run)

    print(report)
    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(report + "\n")

    return 1 if any_hard_failure else 0


if __name__ == "__main__":
    sys.exit(main())
