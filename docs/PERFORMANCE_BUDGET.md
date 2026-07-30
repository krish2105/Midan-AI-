# Midan — Performance Budget

**Primary target: 1080p / 60 fps.** Budget lines are taken from
`docs/ART_DIRECTION.md` §7.1 — the revised MacBook-reality table — **not** the 1440p table
in master prompt §5.1. 1440p is a documented stretch tier, not a gate.

**Status: no measured numbers exist.** Phase 0 sets the targets. Measured columns are
filled in from Phase 8 onward. A number that has not been measured does not go in this
document.

---

## 1. GPU budget

| Line | Budget (ms) | Measured | Verdict |
|---|---|---|---|
| **Total frame** | **16.6** | — | — |
| **GPU total** | **≤ 15.0** | — | — |
| — Base pass (Nanite) | ≤ 3.0 | — | — |
| — Lumen GI + reflections | ≤ 3.5 | — | — |
| — Virtual Shadow Maps | ≤ 2.5 | — | — |
| — Volumetric fog + clouds | ≤ 1.5 | — | — |
| — Post + TSR | ≤ 2.5 | — | — |
| — Translucency / particles | ≤ 1.0 | — | — |

The six sub-lines sum to **14.0 ms**, leaving **1.0 ms unallocated** against the ≤ 15.0 ms
GPU total.

That 1.0 ms is contingency, not spare capacity. It absorbs the variance between a quiet
frame and a worst-case frame — eight cars in frame, tyre smoke, low sun through a canyon
gap. It is not available for a new feature without a written trade against an existing
line.

## 2. CPU budget

`ART_DIRECTION.md` §7.1 revises only the GPU lines. These carry over unchanged from master
prompt §5.1, because dropping from 1440p to 1080p does not relieve game-thread work or
reduce draw-call count (assumption A5 in `docs/ASSUMPTIONS.md`).

| Line | Budget | Measured | Verdict |
|---|---|---|---|
| Game thread | ≤ 6.0 ms | — | — |
| Render thread | ≤ 6.0 ms | — | — |
| RHI thread | ≤ 4.0 ms | — | — |
| Draw calls | ≤ 3,000 | — | — |

## 3. Gate classification

A budget where every line blocks the build gets ignored. A budget where no line blocks it
is decoration. So each line is one of three kinds, and they are treated differently.

### 3.1 Hard gates — binary, block the phase gate and CI

Do not negotiate with a gate. There is no "we improved it to 17.1 ms".

| Gate | Target |
|---|---|
| Total frame time, **p95** across the hot-lap replay | ≤ 16.6 ms |
| Draw calls, peak | ≤ 3,000 |
| Heap allocations inside any physics callback | **0** |
| Game-thread file I/O during telemetry capture | **0** |
| `MidanEditorTools` symbols present in a Shipping binary | **0** |
| PSO cache bundled in the Shipping build | present |

p95, not mean. A mean of 15 ms with a p95 of 24 ms is a game that stutters, and the mean
hides it.

### 3.2 Tuning metrics — report the number and the trend

Each of the six GPU sub-lines and the three thread times. A single sub-line over budget is
a conversation, not a build failure, **provided GPU total still holds** — if Lumen comes in
at 4.0 ms and volumetric fog at 0.8 ms, the frame is fine and the table should say so
rather than flagging a false failure.

### 3.3 Operating metrics — reported at every gate from Phase 8 onward

Package size · cold launch time to main menu · **hitch count before and after PSO
caching** · cold-DDC build time.

These decide whether the slice can actually ship. Excellent frame time with a 40-second
launch and visible shader hitching is not a shippable build.

---

## 4. Measurement provenance

**A number without these five fields is not a number.** Every row in every reported table
states:

1. **Build configuration** — `Test`, never `Development`. Profiling a Development build
   produces numbers that mean nothing about the shipped product.
2. **Resolution and TSR screen percentage** — "1080p" alone is ambiguous when TSR is
   upscaling from 67%.
3. **Machine and GPU** — including OS and driver, since Metal and D3D12 are not
   comparable.
4. **Scalability tier** — which of the four `DefaultScalability.ini` tiers was active.
5. **Run count** — deterministic hot-lap replay, minimum 3 runs. Report p95 **and** max
   delta across runs. Unbounded variance between identical runs means the number is noise.

Further reporting rules:

- **Report failing lines by name.** A table with no failures listed is a table nobody
  believes.
- **Never report an aggregate that hides a segment failure.** "GPU total 14.2 ms" while
  VSM is at 4.1 ms is a misleading pass — break it out.
- **State the limitation you already know about** before a reviewer finds it.
- The deterministic hot-lap replay (Phase 8, replaying recorded telemetry inputs) is what
  makes runs comparable across builds. Without it, every performance claim is a comparison
  between two different laps.

---

## 5. What to cut, in order

From `ART_DIRECTION.md` §7.3. When over budget, cut in this sequence:

1. Volumetric clouds
2. Lumen reflection quality
3. VSM resolution
4. Screen percentage (TSR absorbs a surprising amount)
5. Foliage density

**Never cut film grain, the exposure clamp, or motion blur.** Those are feel, not
fidelity, and cutting them costs more than the frame time they return. Film grain is also
what hides 8-bit banding in the sky gradient — removing it creates a visible artefact, not
just a plainer image.

---

## 6. The three costs that will bite

From `ART_DIRECTION.md` §7.2.

### 6.1 VSM page pool under a low directional light

The art direction locks the sun at **−3° to −6° pitch**, aligned to the main straight, in
an open canyon. Long shadows across a wide open space is precisely the Virtual Shadow Maps
worst case, and this project has chosen it deliberately for the lighting.

Mitigation: clamp `r.Shadow.Virtual.ResolutionLodBiasDirectional` and measure. If VSM
exceeds 2.5 ms, **raise the bias before cutting anything else.**

### 6.2 Volumetric clouds

Frequently 2 ms or more. Measure them in isolation. If they cost more than 1.0 ms, replace
them with a cloud texture on the sky dome — at 250 km/h nobody is studying cloud detail.
The volumetric-fog-and-clouds line is 1.5 ms **combined**, and the fog is the part that
earns its cost: exponential height fog with volumetric enabled is what creates the layered
depth on distant peaks, and it is the cheapest atmosphere available.

### 6.3 Lumen in a wide-open environment

Software tracing (already locked). Clamp max trace distance to what the canyon actually
needs rather than leaving it at default, and lean on the sky light for distant fill.

### 6.4 Metal parity — verify before locking

`ART_DIRECTION.md` §7.4: confirm current Metal feature parity for Nanite, Lumen, and
Virtual Shadow Maps on UE 5.8 / Apple Silicon against Epic's platform documentation
**before** any of these numbers are treated as achievable. This moves between engine
releases.

If any path is materially weaker on Metal than assumed, take the cheaper option **from the
start** rather than discovering it after tuning. Record the finding here — it is
manual step 3 in `docs/MANUAL_STEPS.md`.

---

## 7. Configuration that follows from the budget

Locked at Phase 1, tuned per tier at Phase 8.

| Setting | Decision | Reason |
|---|---|---|
| Nanite | All static track geometry, barriers, buildings, rocks | Not vehicles (skeletal), not translucency, not wind-animated foliage unless measured |
| Lumen | Software tracing default; hardware RT an optional tier | Broad compatibility, better for a 3 km streamed track |
| Lumen quality | Scene Detail and Final Gather set **per scalability level**, not globally | Global settings waste the low tiers |
| VSM | On, with `ResolutionLodBiasDirectional` clamped | See §6.1 |
| Upscaler | TSR | Its history rejection handles high-velocity screen movement better than TAA |
| TSR validation | Verify **no wheel ghosting** at speed; tune `r.TSR.History.ScreenPercentage` | Wheel ghosting is the classic racing-game artefact and a reviewer notices it instantly |
| Motion blur | Object blur on, camera blur low, intensity exposed in settings | A speed-feel system as much as a rendering one — and some players hate it |
| Streaming | World Partition runtime grid sized to the track; HLOD 0 and 1 by commandlet | At 300 km/h the car covers 83 m/s, so streaming distance needs > 250 m of travel-time headroom |
| Materials | Shared master material with instance parameters, **hard limit ≤ 12 unique masters** | Controls shader permutation count and PSO cache size |
| Scalability | Four tiers in `DefaultScalability.ini` + benchmark-driven auto-detect on first run | — |

---

## 8. Reporting template

Phase 8 onward, `Tools/analysis/perf_report.py` emits this shape. Filled in with real
values, never placeholders.

```
MIDAN — FRAME BUDGET REPORT
Config: Test · 1080p @ TSR 67% · Apple M4 Pro / macOS <ver> · Tier: Epic
Hot-lap replay v<n> · 3 runs · reporting p95

HARD GATES
  frame_time_p95_ms              --.-    [<=16.6]      ----
  draw_calls_peak                ----    [<=3000]      ----
  physics_callback_allocations       -    [==0]         ----
  telemetry_gamethread_io            -    [==0]         ----
  editor_module_in_shipping          -    [==0]         ----

TUNING (GPU)
  base_pass_nanite_ms             -.-    [<=3.0]       ----
  lumen_ms                        -.-    [<=3.5]       ----
  virtual_shadow_maps_ms          -.-    [<=2.5]       ----
  volumetric_ms                   -.-    [<=1.5]       ----
  post_tsr_ms                     -.-    [<=2.5]       ----
  translucency_ms                 -.-    [<=1.0]       ----
  gpu_total_ms                    -.-    [<=15.0]      ----

TUNING (CPU)
  game_thread_ms                  -.-    [<=6.0]       ----
  render_thread_ms                -.-    [<=6.0]       ----
  rhi_thread_ms                   -.-    [<=4.0]       ----

OPERATING
  package_size_mb                 ----   [budget TBD]  ----
  cold_launch_seconds             ----   [<=N]         ----
  hitch_count_pre_pso             ----   [baseline]    ----
  hitch_count_post_pso            ----   [< pre]       ----
  run_to_run_max_delta_ms         -.-    [<=1.0]       ----

Failures: <named, or "none">
Known limitations: <stated>
```
