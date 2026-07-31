# Midan — Performance Budget

**Primary target: 1080p / 60 fps.** Budget lines are taken from
`docs/ART_DIRECTION.md` §7.1 — the revised MacBook-reality table — **not** the 1440p table
in master prompt §5.1. 1440p is a documented stretch tier, not a gate.

**Status: no measured numbers exist.** Phase 0 sets the targets. Phase 8 built the
measurement harness (`AMidanHotLapReplay`, `Tools/analysis/perf_report.py`,
`Tools/editor_python/capture_perf_baseline.py`) but has not run it — no engine is installed
in this environment (docs/MANUAL_STEPS.md §0.1). Every "Measured" cell below stays `—` until
a human runs `perf_report.py` against a real capture on real hardware. **The harness
existing is not a measurement.** See docs/MANUAL_STEPS.md Phase 8 for the exact command.

> **Tracks `ART_DIRECTION.md` v2.0** — the neon city pivot with three static time-of-day
> presets. The v1.0 golden-hour canyon allocation is retained below for comparison, because
> the re-allocation is net zero and knowing *why* each line moved is what makes the new
> numbers defensible rather than arbitrary.

---

## 1. GPU budget

| Line | v1.0 | **Budget (ms)** | Measured | Verdict |
|---|---|---|---|---|
| **Total frame** | 16.6 | **16.6** | — | — |
| **GPU total** | ≤ 15.0 | **≤ 15.0** | — | — |
| — Base pass (Nanite) | ≤ 3.0 | **≤ 3.0** | — | — |
| — Lumen GI + reflections | ≤ 3.5 | **≤ 4.0** | — | — |
| — Virtual Shadow Maps | ≤ 2.5 | **≤ 1.5** | — | — |
| — Volumetric fog (clouds cut) | ≤ 1.5 | **≤ 1.0** | — | — |
| — Local lights + emissive | *(absent)* | **≤ 0.75** | — | — |
| — Post + TSR | ≤ 2.5 | **≤ 2.5** | — | — |
| — Translucency / particles / rain | ≤ 1.0 | **≤ 1.25** | — | — |

The seven sub-lines sum to **14.0 ms**, leaving **1.0 ms unallocated** against the ≤ 15.0 ms
GPU total — identical to v1.0. The pivot is a **net-zero re-allocation**: night removes the
low-angle directional light that made Virtual Shadow Maps v1.0's largest risk, and that
1.0 ms saving funds Lumen reflections, emissive signage and rain.

That 1.0 ms is contingency, not spare capacity. It absorbs the variance between a quiet frame
and a worst-case frame — eight cars in frame, wheel spray, rain, and three hero signs
reflecting off standing water. It is not available for a new feature without a written trade
against an existing line.

### 1.1 Hold the budget at the worst preset, per line

Three time-of-day presets means each line has a different worst case, and the budget must
pass on **all three**:

| Line | Worst preset | Why |
|---|---|---|
| Virtual Shadow Maps | **Day** | The only preset with a real sun and a full directional cascade |
| Lumen GI + reflections | **Deep Night** | Emissive-driven indirect plus full standing-water reflection |
| Local lights + emissive | **Deep Night** | All signage lit, rect lights active |
| Translucency / rain | **Deep Night** | Active rain plus wheel spray |
| Volumetric fog | **Deep Night** | Highest fog density, for neon bloom through haze |
| Base pass, Post + TSR | equal | Geometry and resolution do not vary by preset |

A run that passes on Deep Night and fails on Day has not passed. `perf_report.py` (Phase 8)
takes the preset as a parameter and the gate requires three passing runs.

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

From `ART_DIRECTION.md` v2.0 §7.3. When over budget, cut in this sequence:

1. Volumetric clouds — already effectively gone at night; remove entirely
2. **Rain particle density** — the post-process droplet layer carries the effect
3. Lumen reflection **quality** — never wetness itself (see §6.1)
4. **Number of shadow-casting rect lights**
5. VSM resolution
6. Screen percentage (TSR absorbs a surprising amount)
7. Street-level PCG scatter density

**Never cut film grain, the exposure clamp, or motion blur.** Those are feel, not fidelity,
and cutting them costs more than the frame time they return. Film grain matters *more* at
night than it did at golden hour: night scenes are large smooth dark gradients, which band
worse than a bright sky does.

**Never cut the wet-road roughness variation.** It is a material, not a render pass — it
costs essentially nothing and it is the entire look. There is no version of this art
direction with a uniform-roughness road.

---

## 6. The four costs that will bite

From `ART_DIRECTION.md` v2.0 §7.2. Reordered for the neon city — the old number-one risk is
gone, eliminated by the pivot rather than solved.

### 6.1 Lumen reflections on wet asphalt — the new number one

A low-roughness surface covering the entire play area is the worst possible input for a
reflection system, and `ART_DIRECTION.md` §2.3 makes wet road the foundation of the whole
look. This is not a risk to be mitigated away; it is the thing being paid for.

Mitigation, in order: keep **software tracing**; clamp max trace distance to the street
width rather than leaving it at default; reduce reflection **quality** before reducing
wetness. A lower-quality reflection on a wet road still reads as wet. A dry road does not
read at all.

### 6.2 Emissive count and Lumen scene update

Every emissive surface is a potential indirect light source, and §2.2 makes signage the
primary lighting rig. Many small signs cost more than a few large ones for the same visual
result.

Mitigation: consolidate. One large sign beats six small ones, in the profile and
artistically. Measure the Lumen scene-update cost separately from the trace cost — they
scale with different things.

### 6.3 Shadow-casting local lights

Each rect light that casts shadows is a real cost. `ART_DIRECTION.md` §2.2 restricts them to
two or three hero signs deliberately.

Mitigation: if the count creeps past four or five, the `local lights + emissive` line will
blow before any other. Emissive-only signage costs a fraction of a shadow-casting rect light
and covers the great majority of the signage.

### 6.4 Rain particle overdraw

Rain is translucent geometry filling the screen — the textbook overdraw case, and it lands
on the tightest line in the budget (1.25 ms shared with all other translucency and
particles).

Mitigation: measure rain in isolation. Prefer fewer, larger, better-textured streak
particles over many thin ones, and lean on the screen-space droplet layer in post, which is
cheaper and does more for the sensation of rain than the particles do.

### 6.5 Retired from v1.0

Recorded because a reader comparing versions will ask what happened to them:

- **VSM under a low-angle directional light** — was the v1.0 number-one risk. Deep Night has
  the directional light off entirely and Night has it at moonlight intensity, so the
  catastrophic case does not exist in the hero preset. Day remains the VSM worst case but its
  sun is at −25° to −40°, not −3° to −6°, which is a far cheaper cascade. This is where the
  1.0 ms funding the rest of the pivot came from.
- **Volumetric clouds** — cut. At night there is no cloud detail to see. Fog remains and
  remains important; 1.0 ms covers fog alone.

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

Locked at Phase 1. Phase 8 populated the four scalability tiers in
`Config/DefaultScalability.ini` and the measured-clamp settings in `Config/DefaultEngine.ini`
(VSM resolution bias, Lumen trace distance, TSR history) — as **unmeasured starting points**,
not tuned results. Real tuning happens once the Phase 8 gate has run on real hardware.

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
