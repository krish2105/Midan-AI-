#!/usr/bin/env python3
"""Turn a Midan telemetry CSV capture into an HTML report with six charts.

Usage:

    python3 Tools/analysis/telemetry_report.py \\
        --player Saved/Telemetry/<capture>_Player.csv \\
        --ai Saved/Telemetry/<capture>_AI01.csv \\
        --out Saved/Telemetry/report.html

`--player` is required; `--ai` is optional and enables chart 6
(player-vs-AI delta trace). CSVs come from
UMidanTelemetrySubsystem::ExportCaptureToCsv — see docs/MANUAL_STEPS.md
Phase 9 for the capture -> export -> report sequence.

Deliberately stdlib-only — no matplotlib, no pandas, no numpy. Every other
Tools/ script in this project is either `unreal`-only (editor Python) or
plain stdlib, and a hidden dependency on a package that has to be pip
installed before this ever runs is exactly the kind of friction that makes a
"run the report" step get skipped. Charts are hand-built inline SVG.

Reporting discipline (.claude/skills/defensible-data-analysis and the Phase 9
gate): every chart states its sample count. Laps are only compared to other
laps of the same racer, on the same track, never mixed across a car swap.
No metric appears without stating what it is a fraction/rate of.
"""

import argparse
import csv
import statistics
import sys
from collections import defaultdict


SVG_WIDTH = 900
SVG_HEIGHT = 260
SVG_MARGIN = 40


def load_csv(path):
    """Return a list of dict rows with numeric fields converted to float/int."""
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for raw in reader:
            row = dict(raw)
            for key, value in raw.items():
                if key in ("source_id",) or key.startswith("surface_"):
                    continue
                try:
                    row[key] = float(value)
                except ValueError:
                    pass
            rows.append(row)
    return rows


def svg_polyline(points, color, width=SVG_WIDTH, height=SVG_HEIGHT, margin=SVG_MARGIN,
                  x_range=None, y_range=None):
    """points: list of (x, y). Returns an SVG <polyline> string scaled to fit."""
    if not points:
        return ""

    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    x_min, x_max = x_range if x_range else (min(xs), max(xs))
    y_min, y_max = y_range if y_range else (min(ys), max(ys))
    x_span = (x_max - x_min) or 1.0
    y_span = (y_max - y_min) or 1.0

    def sx(x):
        return margin + (x - x_min) / x_span * (width - 2 * margin)

    def sy(y):
        return height - margin - (y - y_min) / y_span * (height - 2 * margin)

    coords = " ".join(f"{sx(x):.1f},{sy(y):.1f}" for x, y in points)
    return f'<polyline points="{coords}" fill="none" stroke="{color}" stroke-width="1.5" />'


def svg_chart(title, sample_count_note, body_svg, x_range=None, y_range=None):
    axis_note = ""
    if x_range:
        axis_note += f" x:[{x_range[0]:.0f},{x_range[1]:.0f}]"
    if y_range:
        axis_note += f" y:[{y_range[0]:.1f},{y_range[1]:.1f}]"

    return f"""
    <div class="chart">
      <h3>{title}</h3>
      <p class="note">{sample_count_note}{axis_note}</p>
      <svg width="{SVG_WIDTH}" height="{SVG_HEIGHT}" viewBox="0 0 {SVG_WIDTH} {SVG_HEIGHT}">
        <rect x="0" y="0" width="{SVG_WIDTH}" height="{SVG_HEIGHT}" fill="#111318" />
        {body_svg}
      </svg>
    </div>
    """


def chart_speed_trace(rows):
    points = [(r["track_arc_cm"], r["speed_kmh"]) for r in rows]
    body = svg_polyline(points, "#e0c341")
    return svg_chart(
        "1. Speed trace vs distance",
        f"n={len(rows)} samples, 1 racer, distance = track arc length (cm).",
        body,
    )


def chart_throttle_brake(rows):
    throttle_pts = [(r["track_arc_cm"], r["throttle"]) for r in rows]
    brake_pts = [(r["track_arc_cm"], r["brake"]) for r in rows]
    body = svg_polyline(throttle_pts, "#3fae5c", y_range=(0.0, 1.0)) + \
        svg_polyline(brake_pts, "#c0392b", y_range=(0.0, 1.0))
    return svg_chart(
        "2. Throttle / brake overlay",
        f"n={len(rows)} samples. Green=throttle, red=brake, both 0..1.",
        body,
        y_range=(0.0, 1.0),
    )


def chart_deviation_heatmap(rows):
    """Racing-line deviation, coloured by |lateral offset| magnitude."""
    if not rows:
        return svg_chart("3. Racing-line deviation heat map", "n=0 samples — no data.", "")

    max_abs = max(abs(r["track_lateral_cm"]) for r in rows) or 1.0
    x_min = min(r["track_arc_cm"] for r in rows)
    x_max = max(r["track_arc_cm"] for r in rows)
    x_span = (x_max - x_min) or 1.0

    circles = []
    for r in rows:
        x = SVG_MARGIN + (r["track_arc_cm"] - x_min) / x_span * (SVG_WIDTH - 2 * SVG_MARGIN)
        intensity = min(abs(r["track_lateral_cm"]) / max_abs, 1.0)
        # Green (on line) -> amber -> red (furthest off line).
        red = int(80 + intensity * 175)
        green = int(200 - intensity * 160)
        circles.append(f'<circle cx="{x:.1f}" cy="{SVG_HEIGHT / 2:.1f}" r="2" fill="rgb({red},{green},60)" />')

    return svg_chart(
        "3. Racing-line deviation heat map",
        f"n={len(rows)} samples. Colour = |lateral offset| from track centre, max observed {max_abs:.0f}cm.",
        "".join(circles),
    )


def chart_slip_angle_distribution(rows):
    """Per-sector max |slip angle| across all four wheels, as a bar per sector."""
    by_sector = defaultdict(list)
    for r in rows:
        max_slip = max(abs(r.get(f"slip_angle_{w}", 0.0)) for w in ("fl", "fr", "rl", "rr"))
        by_sector[int(r["sector"])].append(max_slip)

    if not by_sector:
        return svg_chart("4. Slip-angle distribution per corner (by sector)", "n=0 samples — no data.", "")

    sectors = sorted(by_sector.keys())
    max_value = max(max(v) for v in by_sector.values()) or 1.0
    bar_width = (SVG_WIDTH - 2 * SVG_MARGIN) / max(len(sectors), 1) * 0.6

    bars = []
    for i, sector in enumerate(sectors):
        values = by_sector[sector]
        mean_slip = statistics.mean(values)
        p95_slip = sorted(values)[int(len(values) * 0.95)] if len(values) > 1 else values[0]
        x = SVG_MARGIN + i * (SVG_WIDTH - 2 * SVG_MARGIN) / len(sectors)
        mean_h = mean_slip / max_value * (SVG_HEIGHT - 2 * SVG_MARGIN)
        p95_h = p95_slip / max_value * (SVG_HEIGHT - 2 * SVG_MARGIN)
        bars.append(f'<rect x="{x:.1f}" y="{SVG_HEIGHT - SVG_MARGIN - p95_h:.1f}" width="{bar_width:.1f}" height="{p95_h:.1f}" fill="#5566aa" />')
        bars.append(f'<rect x="{x:.1f}" y="{SVG_HEIGHT - SVG_MARGIN - mean_h:.1f}" width="{bar_width:.1f}" height="{mean_h:.1f}" fill="#e0c341" />')

    note = (f"n={len(rows)} samples across {len(sectors)} sector(s) (proxy for 'per corner' — "
            f"this vertical slice buckets by sector, not individually detected corners). "
            f"Amber=mean, blue cap=p95, per sector, degrees.")
    return svg_chart("4. Slip-angle distribution per corner (by sector)", note, "".join(bars))


def compute_sector_times(rows):
    """Sector completion time = timestamp at the first sample of the NEXT sector,
    walking forward through the capture in timestamp order. Returns
    {sector_index: [duration_seconds, ...]} across every completed instance."""
    rows_sorted = sorted(rows, key=lambda r: r["timestamp_s"])
    durations = defaultdict(list)
    sector_start_time = None
    current_sector = None

    for r in rows_sorted:
        sector = int(r["sector"])
        if current_sector is None:
            current_sector = sector
            sector_start_time = r["timestamp_s"]
            continue
        if sector != current_sector:
            durations[current_sector].append(r["timestamp_s"] - sector_start_time)
            current_sector = sector
            sector_start_time = r["timestamp_s"]

    return durations


def chart_sector_consistency(rows):
    durations = compute_sector_times(rows)
    if not durations:
        return svg_chart("5. Sector-time consistency", "n=0 completed sectors — no data.", "")

    lines = []
    for sector in sorted(durations.keys()):
        times = durations[sector]
        n = len(times)
        mean_t = statistics.mean(times)
        stdev_t = statistics.pstdev(times) if n > 1 else 0.0
        lines.append(f"Sector {sector}: n={n}, mean={mean_t:.3f}s, stdev={stdev_t:.3f}s")

    total_completed = sum(len(v) for v in durations.values())
    # Rendered as a text table rather than a bar chart — stdev across only a
    # handful of completed sectors (one race = LapCount completions) is more
    # honestly read as numbers than as a bar whose height implies more
    # precision than a small n actually supports.
    rows_html = "".join(f"<div>{line}</div>" for line in lines)
    return f"""
    <div class="chart">
      <h3>5. Sector-time consistency</h3>
      <p class="note">n={total_completed} completed sector instances across {len(durations)} sector(s) — matched to this one racer's own capture only, never compared across different vehicles or difficulty tiers.</p>
      <div class="table">{rows_html}</div>
    </div>
    """


def chart_player_vs_ai_delta(player_rows, ai_rows):
    if not ai_rows:
        return svg_chart("6. Player-vs-AI delta trace", "No --ai file supplied — skipped.", "")

    # Interpolate each dataset's elapsed time as a function of track arc
    # length, then delta = ai_time(arc) - player_time(arc). Matched
    # condition: same arc-length axis, both racers on the same (one) track.
    def time_at_arc(rows_sorted, arc):
        for i in range(len(rows_sorted) - 1):
            a0, a1 = rows_sorted[i]["track_arc_cm"], rows_sorted[i + 1]["track_arc_cm"]
            if a0 <= arc <= a1 and a1 > a0:
                alpha = (arc - a0) / (a1 - a0)
                t0, t1 = rows_sorted[i]["timestamp_s"], rows_sorted[i + 1]["timestamp_s"]
                return t0 + alpha * (t1 - t0)
        return None

    player_sorted = sorted(player_rows, key=lambda r: r["track_arc_cm"])
    ai_sorted = sorted(ai_rows, key=lambda r: r["track_arc_cm"])

    arc_min = max(min(r["track_arc_cm"] for r in player_sorted), min(r["track_arc_cm"] for r in ai_sorted))
    arc_max = min(max(r["track_arc_cm"] for r in player_sorted), max(r["track_arc_cm"] for r in ai_sorted))

    samples = 100
    points = []
    for i in range(samples + 1):
        arc = arc_min + (arc_max - arc_min) * i / samples
        t_player = time_at_arc(player_sorted, arc)
        t_ai = time_at_arc(ai_sorted, arc)
        if t_player is not None and t_ai is not None:
            points.append((arc, t_ai - t_player))

    body = svg_polyline(points, "#e0c341")
    return svg_chart(
        "6. Player-vs-AI delta trace",
        f"n={len(points)} interpolated samples over the shared arc-length range "
        f"[{arc_min:.0f},{arc_max:.0f}]cm. Positive = AI slower to reach that point than the player.",
        body,
    )


def build_report(player_rows, ai_rows, player_path, ai_path):
    charts = [
        chart_speed_trace(player_rows),
        chart_throttle_brake(player_rows),
        chart_deviation_heatmap(player_rows),
        chart_slip_angle_distribution(player_rows),
        chart_sector_consistency(player_rows),
        chart_player_vs_ai_delta(player_rows, ai_rows),
    ]

    style = """
    body { background:#0b0c10; color:#ddd; font-family: -apple-system, sans-serif; margin: 24px; }
    h1 { font-weight: 600; }
    .chart { background:#15171c; border-radius:8px; padding:16px; margin-bottom:20px; }
    .note { color:#999; font-size:12px; margin: 4px 0 12px; }
    .table { font-family: monospace; font-size: 13px; }
    """

    return f"""<!doctype html>
<html><head><meta charset="utf-8"><title>Midan Telemetry Report</title>
<style>{style}</style></head>
<body>
<h1>Midan Telemetry Report</h1>
<p class="note">Player capture: {player_path} ({len(player_rows)} samples). {"AI capture: " + ai_path + f" ({len(ai_rows)} samples)." if ai_rows else "No AI capture supplied."}</p>
{"".join(charts)}
</body></html>
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--player", required=True, help="Path to the player's exported CSV.")
    parser.add_argument("--ai", default=None, help="Path to an AI opponent's exported CSV (enables chart 6).")
    parser.add_argument("--out", default="telemetry_report.html", help="Output HTML path.")
    args = parser.parse_args()

    player_rows = load_csv(args.player)
    if not player_rows:
        print(f"ERROR: {args.player} has no rows.", file=sys.stderr)
        return 1

    ai_rows = load_csv(args.ai) if args.ai else []

    html = build_report(player_rows, ai_rows, args.player, args.ai or "")

    with open(args.out, "w", encoding="utf-8") as f:
        f.write(html)

    print(f"Wrote {args.out} ({len(player_rows)} player samples, {len(ai_rows)} AI samples).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
