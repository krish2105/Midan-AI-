# Midan

**ميدان — "the arena / the field"**

A racing vertical slice in Unreal Engine 5. One 3 km circuit, three vehicles with
deliberately different handling personalities, seven AI opponents, full race flow, 60 Hz
telemetry capture, a profiling harness, and a scripted build pipeline.

> **Status: files written for all 10 phases · every gate BLOCKED on engine install.** No
> Unreal Engine install exists in the environment these phases were authored in, so nothing
> has been compiled, run, or measured — see `docs/MANUAL_STEPS.md` §0.1 and each phase's
> status line in `docs/PHASE_PLAN.md`. This README carries no performance figures because
> none have been measured; CLAUDE.md's measurement rule means that stays true regardless of
> how complete the code is. See `docs/PERFORMANCE_BUDGET.md`.

---

## What this project is trying to demonstrate

Three things, in order:

1. **Every tuning value lives in a Data Asset, not in C++.** Handling is diffable in version
   control, validatable in CI, and sweepable programmatically. No gameplay-relevant
   hardcoded float exists in the codebase.
2. **The AI drives through the identical input path as the player.** Pure-pursuit lateral
   control and a PID longitudinal controller against a speed profile generated from
   curvature with a backward braking pass. No physics cheating, structurally enforced:
   there is one `ApplyInput` entry point and no second path.
3. **A documented frame budget held with a deterministic hot-lap replay**, so performance
   numbers are comparable across builds rather than being comparisons between two different
   laps.

## Architecture

Six C++ modules, no circular dependencies, cross-module reads through interfaces, plus one
functional-test module (Phase 10) that never ships:

```
MidanCore → nothing            shared types, interfaces, tags, math
MidanVehicle → Core            pawn, physics config, feel systems
MidanRace → Core               track, checkpoints, lap timing, race flow
MidanTelemetry → Core          capture, serialisation, ghost replay
MidanAI → Core + Vehicle       racing line, opponent control
MidanEditorTools → all         validators, spline tools (EDITOR TARGET ONLY)
MidanTests → Core, Vehicle,    map-based functional tests
             Race, AI          (Development/Test builds only, never Shipping)
```

Full class inventory and rationale: **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)**

## Stack

UE 5.8 · Chaos Vehicles · Enhanced Input · World Partition + HLOD · Nanite · Lumen
(software tracing) · Virtual Shadow Maps · TSR · MetaSounds · PCG · Gameplay Tags · async
physics tick with 120 Hz substepping.

## Documentation

| Document | Contents |
|---|---|
| [CLAUDE.md](CLAUDE.md) | Locked stack, the C++/Data/Blueprint split, forbidden actions |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Module graph, class inventory, tick inventory |
| [docs/PERFORMANCE_BUDGET.md](docs/PERFORMANCE_BUDGET.md) | 1080p/60 budget, gate classification, provenance rules |
| [docs/PHASE_PLAN.md](docs/PHASE_PLAN.md) | File-level plan for all ten phases |
| [docs/MANUAL_STEPS.md](docs/MANUAL_STEPS.md) | Everything the editor or Blender is needed for |
| [docs/ASSUMPTIONS.md](docs/ASSUMPTIONS.md) | Assumptions, ambiguities, open decisions |
| [docs/VEHICLE_SPEC.md](docs/VEHICLE_SPEC.md) | The three-car roster |
| [docs/ART_DIRECTION.md](docs/ART_DIRECTION.md) | Lighting, materials, camera, HUD, post stack |
| [docs/ASSET_LICENCES.md](docs/ASSET_LICENCES.md) | Third-party asset ledger |
| [docs/BUILD_RUNBOOK.md](docs/BUILD_RUNBOOK.md) | Clean checkout → verified, uploaded Shipping build |

## Building

Requires **Unreal Engine 5.8** and Xcode (macOS) or Visual Studio 2022 with the "Game
development with C++" workload (Windows). See `docs/MANUAL_STEPS.md` §0.1.

Generate project files and build the editor target:

```bash
/Users/Shared/Epic\ Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh MidanEditor Mac Development -project="$PWD/Midan.uproject"
```

The full scripted build, PSO-cache, and verification pipeline is one command:

```bash
python3 Tools/build/package_shipping.py --platform Mac
```

See **[docs/BUILD_RUNBOOK.md](docs/BUILD_RUNBOOK.md)** for the complete path from a clean
checkout to an uploaded Shipping build, and for how CI (`.github/workflows/ci.yml`,
self-hosted runners, Mac lane active / Win64 lane gated on runner availability) wires it
together.

## Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Throttle | `W` | Right trigger |
| Brake | `S` | Left trigger |
| Steer | `A` / `D` | Left stick |
| Handbrake | `Space` | `A` / Cross |
| Shift up / down | `E` / `Q` | Right / left bumper |
| Look back | `C` | Right stick click |
| Cycle camera mode | `V` | D-pad up |
| Respawn | `R` | D-pad down |
| Pause | `Esc` | Menu / Start |

Authored via `IMC_MidanDriving` (`docs/MANUAL_STEPS.md` §1.3) — bindings are data, not code,
and can be remapped without touching `UVehicleInputComponent`.

## Minimum specs

**Not yet measured** — no hardware profiling has run (see the status banner above). The
scalability tiers in `Config/DefaultScalability.ini` and the render settings in
`Config/DefaultEngine.ini` are explicitly labelled `UNMEASURED STARTING POINT` pending the
Phase 8 gate; this section will state real minimum and recommended specs once that gate has
a result to report, per `docs/PERFORMANCE_BUDGET.md` §4's provenance rule.

## Performance

Target **1080p / 60 fps**, with 1440p as a stretch tier. The measurement harness exists
(`AMidanHotLapReplay`, `Tools/analysis/perf_report.py`, `Tools/build/generate_pso_cache.py`)
but has not been run — **the harness existing is not a measurement.** The table goes here,
with its full provenance (build configuration, resolution and TSR screen percentage,
machine, scalability tier, and run count) plus the before/after PSO-cache hitch-count pair,
the first time `docs/BUILD_RUNBOOK.md` §9 actually runs against real hardware. Until then
this section stays empty rather than carrying an estimate — see CLAUDE.md's measurement
rule.

## Known issues

- **Nothing has been compiled.** Every phase in `docs/PHASE_PLAN.md` reads "files written,
  gate BLOCKED on engine install" — this is source code that has never seen a compiler.
  Expect a first-compile error list; each phase's `docs/MANUAL_STEPS.md` section names the
  most likely candidates.
- **Every `// API VERIFY` comment in the codebase** marks a Chaos Vehicles, async-physics,
  Enhanced Input, or World Partition call whose exact 5.8 signature is unconfirmed. Search
  for the marker; there is no hidden list elsewhere.
- **All Blender/editor/Blueprint content work is undone.** Vehicle meshes, the track spline
  and its road mesh, lighting rigs for the three time-of-day presets, MetaSound graphs, and
  every `WBP_` UI Blueprint are described in `docs/MANUAL_STEPS.md` but do not exist as
  assets yet.
- **The Win64 build/CI lane is gated off**, not broken — no Windows host or runner exists
  yet (`docs/ASSUMPTIONS.md` A4, A19). `Tools/build/build.py --platform Win64` and the
  corresponding CI job are ready to enable the moment one does.

## Asset attributions

No third-party asset in this project currently requires a shipped attribution — see the
full ledger in [docs/ASSET_LICENCES.md](docs/ASSET_LICENCES.md). Two AI-generated concept
reference images live under `docs/concept/` as design references only (never shipped in
`Content/`); their generation model and known limitations are recorded there. This section
is regenerated from that ledger whenever a CC-BY-licensed asset is added — per
`docs/ASSET_LICENCES.md`'s own rule, an added row with no matching entry here is a licence
breach, not an oversight to fix later.

## Legal

All vehicles are original fictional designs. No real manufacturer name, model name, badge,
or brand-identifying light signature appears anywhere in this project. Third-party asset
licences are recorded in [docs/ASSET_LICENCES.md](docs/ASSET_LICENCES.md).
