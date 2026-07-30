# CLAUDE.md — Midan

UE5 racing vertical slice. One 3 km circuit, three vehicles, seven AI opponents,
telemetry, a profiling harness, an automated build pipeline.

**Read `docs/ARCHITECTURE.md` before adding a class. Read `docs/PERFORMANCE_BUDGET.md`
before adding anything that costs frame time.**

---

## Locked stack — do not relitigate

| Area | Decision |
|---|---|
| Engine | **UE 5.8** |
| Vehicle physics | **Chaos Vehicles** (`ChaosVehiclesPlugin`) |
| Input | **Enhanced Input** — never legacy bindings |
| Streaming | **World Partition** + HLOD levels 0 and 1 |
| Static geometry | **Nanite** — not vehicles, not translucency, not wind-animated foliage |
| GI + reflections | **Lumen, software tracing** default; hardware RT as an optional tier |
| Shadows | **Virtual Shadow Maps** |
| Upscaler | **TSR** — not TAA, not FSR |
| Audio | **MetaSounds** — zero external dependencies |
| Foliage | **PCG** |
| State | **Gameplay Tags** — never string comparison |
| Physics timing | **Async physics tick + substepping at 120 Hz** — non-negotiable |

Primary performance target is **1080p / 60 fps**. 1440p is a documented stretch tier, not
a gate. See `docs/PERFORMANCE_BUDGET.md`.

---

## The C++ / Data / Blueprint split — the most important rule in this repo

| Lives in | Contains | Never contains |
|---|---|---|
| **C++** | Systems, algorithms, components, subsystems, interfaces, math | Tuning numbers |
| **Data Assets** | Every tuning value: torque curves, friction, camera curves, AI difficulty | Logic |
| **Blueprint** | Thin subclasses of C++ classes, asset references, cosmetic FX wiring | Systems, math, gameplay rules |

**Rationale:** a designer tuning by feel must change how a car drives without
recompiling. A programmer must change how physics is applied without touching a
Blueprint graph.

### Corollary — no hardcoded tuning value

No gameplay-relevant hardcoded float in C++. Every constant is either:

- a `UPROPERTY(EditDefaultsOnly)` on a Data Asset, **or**
- a named `constexpr` with a comment stating **why it can never change**.

If a human would ever want to tune it by feel, it belongs in a Data Asset. Full stop.
Every tuning Data Asset derives from `UMidanDataAsset` and overrides `ValidateData()`.

---

## Module graph

```
MidanCore → nothing (engine only)
MidanVehicle → Core
MidanRace → Core
MidanTelemetry → Core
MidanAI → Core + Vehicle
MidanEditorTools → all of the above (Editor target ONLY)
```

**No module hard-links a sibling.** Cross-module reads go through interfaces declared in
`MidanCore` and resolved via `UMidanServiceLocatorSubsystem`. Full graph and rationale in
`docs/ARCHITECTURE.md` §2.1.

Changing this graph is a **stop condition** — ask first.

---

## Forbidden actions

- **NEVER** write gameplay logic in Blueprint. Blueprints are thin data-carrying subclasses only.
- **NEVER** hardcode a tuning value in C++.
- **NEVER** use `Tick` where a timer, event, or the physics callback will do. State every per-frame cost you introduce.
- **NEVER** allocate in a physics callback or per-frame hot path.
- **NEVER** use `FindObject`, `LoadObject`, or hard references in constructors. Use `TSoftObjectPtr` and async loading.
- **NEVER** commit `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`.
- **NEVER** commit a binary asset without Git LFS configured for its extension.
- **NEVER** reference a real car manufacturer, model name, or logo. Vehicles are original fictional designs. **This is a legal boundary, not a stylistic one.**
- **NEVER** use an asset without recording its licence in `docs/ASSET_LICENCES.md`.
- **NEVER** add a plugin or third-party dependency without asking.
- **NEVER** `git push`, tag a release, or upload a build without asking.
- **NEVER** claim a performance number you have not measured.

---

## Stop conditions — pause and ask before

- deleting a file
- adding a plugin or third-party dependency
- changing the module dependency graph
- changing the frame budget targets
- anything requiring the editor that you might be tempted to fake
- any architecture fork with two valid paths
- any error unresolved after 2 attempts
- moving to the next phase

---

## Code conventions

- `TObjectPtr<T>`, never a raw `UObject*` member.
- `TSoftObjectPtr<T>` for any asset reference that is not needed immediately.
- Forward-declare in headers; include in `.cpp`. Keep compile times sane.
- Const-correct. `virtual` and `override` explicit, never implied.
- Every class header carries a comment stating **its responsibility** and **its single
  reason to change**.
- `UPROPERTY` / `UFUNCTION` macros complete and correct — category, edit specifier, meta.
- No TODOs. No stubbed function presented as complete. If it needs the editor, it goes in
  `docs/MANUAL_STEPS.md`.
- Every system that ticks justifies its tick at the phase gate. The pre-committed tick
  inventory is `docs/ARCHITECTURE.md` §2.3 — additions need justification.
- Pure math lives in free functions or plain structs with no `UWorld` dependency, so it is
  unit-testable by an Automation Spec without a map.

### Naming

- Public types carry the project prefix: `UMidan…`, `AMidan…`, `FMidan…`, `IMidan…`.
- Vehicle config structs follow the master-prompt convention instead: `FVehicleMassConfig`,
  `FVehiclePowertrainConfig`, and so on.
- Native gameplay tags are declared once in `MidanGameplayTags.h`. Never construct a tag
  from a string literal at a call site.

---

## Legal boundary

No real manufacturer, model name, badge, grille shape traceable to a marque, or
recognisable light signature. From a reference you may borrow *design language*, never
geometry. If a car enthusiast can name it, redesign it.

Record every third-party asset and its licence in `docs/ASSET_LICENCES.md` on the same
commit that adds the asset.

---

## Measurement rule

A number you have not measured does not go in a table, a README, or a commit message.

Every reported performance figure states five things: **build configuration** (`Test`,
never `Development`), **resolution and TSR screen percentage**, **machine and GPU**,
**scalability tier**, and **run count**. Report failing lines by name. See
`docs/PERFORMANCE_BUDGET.md` §3.4.

---

## Repo map

| Path | Contents |
|---|---|
| `Source/` | Six modules + two `.Target.cs` |
| `Config/` | `DefaultEngine.ini`, `DefaultGame.ini`, `DefaultInput.ini`, `DefaultScalability.ini` |
| `Content/` | Editor-authored, LFS-tracked |
| `Tools/editor_python/` | UE Python editor automation |
| `Tools/build/` | Build, package, PSO cache, verification |
| `Tools/analysis/` | Telemetry and performance reporting |
| `docs/` | Architecture, budget, phase plan, manual steps, assumptions, licences, runbook |
| `.claude/skills/` | `unreal-cpp-architecture`, `vehicle-physics-tuning`, `realtime-performance-budget` |

## Phase discipline

Ten phases, each ending in a gate. Current status and the file-level plan are in
`docs/PHASE_PLAN.md`. Every gate outputs: a `✅` summary, files created, **measured**
numbers where applicable, and a new `docs/MANUAL_STEPS.md` section.

Only do what the current phase requires. No speculative abstraction.
