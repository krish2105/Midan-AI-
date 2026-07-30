# Midan

**ميدان — "the arena / the field"**

A racing vertical slice in Unreal Engine 5. One 3 km circuit, three vehicles with
deliberately different handling personalities, seven AI opponents, full race flow, 60 Hz
telemetry capture, a profiling harness, and a scripted build pipeline.

> **Status: Phase 1 of 10 — project scaffold.** Not yet playable. This README carries no
> performance figures because none have been measured; the measured budget table is
> produced at Phase 8 and finalised at Phase 10. See `docs/PERFORMANCE_BUDGET.md`.

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

Six C++ modules, no circular dependencies, cross-module reads through interfaces:

```
MidanCore → nothing            shared types, interfaces, tags, math
MidanVehicle → Core            pawn, physics config, feel systems
MidanRace → Core               track, checkpoints, lap timing, race flow
MidanTelemetry → Core          capture, serialisation, ghost replay
MidanAI → Core + Vehicle       racing line, opponent control
MidanEditorTools → all         validators, spline tools (EDITOR TARGET ONLY)
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

## Building

Requires **Unreal Engine 5.8** and Xcode (macOS) or Visual Studio 2022 with the "Game
development with C++" workload (Windows). See `docs/MANUAL_STEPS.md` §0.1.

Generate project files and build the editor target:

```bash
/Users/Shared/Epic\ Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh MidanEditor Mac Development -project="$PWD/Midan.uproject"
```

A scripted build, package, and verification pipeline arrives at Phase 10
(`Tools/build/build.py`).

## Performance

Target **1080p / 60 fps**, with 1440p as a stretch tier. The measured table goes here at
Phase 8 with its full provenance — build configuration, resolution and TSR screen
percentage, machine, scalability tier, and run count. Until then this section stays empty
rather than carrying an estimate.

## Legal

All vehicles are original fictional designs. No real manufacturer name, model name, badge,
or brand-identifying light signature appears anywhere in this project. Third-party asset
licences are recorded in [docs/ASSET_LICENCES.md](docs/ASSET_LICENCES.md).
