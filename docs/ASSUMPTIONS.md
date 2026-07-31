# Midan — Assumptions & Ambiguities

Every assumption made during Phase 0 and every ambiguity found in the source documents.
Anything marked **open** needs a decision from you before the phase that depends on it.

Source documents: `MIDAN_UE5_RACING_CLAUDE_CODE_MASTER_PROMPT.md`, `docs/ART_DIRECTION.md`,
`docs/VEHICLE_SPEC.md`.

---

## Resolved with the user

### A1 — The repo has no `Source/`, `Config/`, or `.uproject`

**Found:** the Phase 0 brief stated that the project was generated from the UE5 Blank C++
template and that `Source/` and `Config/` held the default single-module layout. Neither
directory exists. `git ls-files` returns five files; the commit titled *"add UE project
skeleton"* (`0244607`) only edited `docs/VEHICLE_SPEC.md`. `Tools/analysis`,
`Tools/build`, and `Tools/editor_python` exist but are empty (git does not track empty
directories). No engine is installed.

**Resolved:** Phase 1 is **greenfield creation** of the six-module layout, not a
restructure. No file is moved, renamed, or deleted, because none exists. A Blank-template
deletion map is recorded in `docs/PHASE_PLAN.md` Phase 1 so that if the template is
generated later, the migration is mechanical.

### A2 — Three of the named skills do not exist

**Found:** `unreal-cpp-architecture`, `vehicle-physics-tuning`, and
`realtime-performance-budget` are not installed. Master prompt §0.3 anticipates exactly
this and recommends having `unreal-cpp-architecture` authored as the first Phase 0
deliverable.

**Resolved:** all three authored in Phase 0 under `.claude/skills/`, so the conventions
survive across sessions rather than living only in `CLAUDE.md`.

### A3 — Engine version was unspecified beyond "5.5+"

**Found:** master prompt §0.4 says "UE 5.5 or newer". No engine installed. Both other UE
projects on this machine (`UEIntroProject`, `ProjectEmber`) are `EngineAssociation: 5.8`.

**Resolved:** target **UE 5.8**.

**Risk to expect:** knowledge of 5.8-specific Chaos Vehicles and `Build.cs` API detail is
thinner than for 5.5/5.6. Some signatures will need correction at the first compile. That is
a Phase 1 gate item, not a surprise — do not treat a first-compile error list as a design
failure.

### A4 — Windows shipping target vs an Apple Silicon dev machine

**Found:** the master prompt requires a Windows Shipping executable (§7.2) and a
self-hosted **Windows** CI runner (§7.6). §0.4 acknowledges the Mac case and says to
develop on Mac and produce the Windows build elsewhere.

**Resolved:** Mac-first through Phase 9. Phase 10's `build.py` and `package_shipping.py`
are platform-parameterised (`--platform Mac|Win64`). The CI workflow defines both lanes with
Win64 gated on runner availability. The Phase 10 gate reports Mac numbers, labelled as such.

**Consequence:** the acceptance criterion "Shipping build produced by one command" is met on
Mac at Phase 10. The Windows criterion stays open until a Windows host exists. Related: A19.

---

## Resolved unilaterally — stated, not asked. Flag any you disagree with.

### A5 — `ART_DIRECTION.md` §7.1 omits the CPU budget lines

§7.1 revises only GPU lines. It has no game-thread, render-thread, RHI-thread, or draw-call
row. Master prompt §5.1 has all four.

**Assumed:** carried over unchanged — game thread ≤ 6.0 ms, render thread ≤ 6.0 ms, RHI
thread ≤ 4.0 ms, draw calls ≤ 3,000. Dropping the render resolution from 1440p to 1080p
does not relieve game-thread work and does not reduce draw-call count, so there is no basis
for loosening them and good reason to keep them.

### A6 — §7.1 both tightens and extends the master-prompt budget

Per your instruction, §7.1 wins on every conflicting line:

| Line | Master prompt §5.1 (1440p) | ART_DIRECTION §7.1 (1080p) | Using |
|---|---|---|---|
| Base pass (Nanite) | ≤ 3.5 | ≤ 3.0 | **3.0** |
| Lumen GI + reflections | ≤ 4.0 | ≤ 3.5 | **3.5** |
| Virtual Shadow Maps | ≤ 2.5 | ≤ 2.5 | 2.5 |
| Volumetric fog + clouds | *(absent)* | ≤ 1.5 | **1.5** |
| Post + TSR | ≤ 2.5 | ≤ 2.5 | 2.5 |
| Translucency / particles | ≤ 1.5 | ≤ 1.0 | **1.0** |

§7.1 introduces the volumetric line the master prompt lacks — correctly, since the art
direction commits to volumetric height fog and clouds. Sub-lines sum to 14.0 ms against a
≤ 15.0 ms GPU total, so 1.0 ms is unallocated contingency.

### A7 — `MidanRace`'s module dependencies are never stated

The master prompt fixes Core's, Vehicle's, AI's, and EditorTools' dependencies. It never
says what `MidanRace` may depend on.

**Assumed:** `MidanCore` only. Race reads vehicles through `IMidanVehicleInterface`,
resolved via `UMidanServiceLocatorSubsystem`.

**Why:** it keeps the stated graph literal (changing the graph is a stop condition), and it
makes `MidanRace` compile and unit-test without `MidanVehicle` — which matters because lap
validation and position calculation are the two things with Automation Specs at Phase 5.

**This is the single architecture decision most worth your objection.** `MidanRace →
MidanVehicle` is simpler: no interface, no locator, direct calls. The cost is that Race
can no longer be tested in isolation and the two modules become a unit. If you prefer the
simpler version, say so before Phase 5 — after that, the interface is load-bearing.

### A8 — `MidanTelemetry`'s module dependencies are never stated

**Assumed:** `MidanCore` only. Telemetry pulls per-vehicle state through
`IMidanTelemetrySource`, which `AMidanVehiclePawn` implements. Telemetry never links
`MidanVehicle`, which is what lets `TelemetryRoundTripSpec` run without spawning a car.

### A9 — `MidanAI` needs race state but the graph gives it only Core + Vehicle

An opponent must not drive during `Race.State.Countdown` and should coast after
`Race.State.Finished`. That is race information, and the master prompt's graph gives AI
only Core and Vehicle.

**Assumed:** AI reads `IMidanRaceStateInterface` from Core rather than adding `MidanRace` to
its dependency list — which would be a graph change, hence a stop condition.

### A10 — Rubber-band logging implies AI → Telemetry

Master prompt §4.4 requires every rubber-band application logged to telemetry so its
subtlety is provable. That is a dependency the graph does not grant.

**Assumed:** routed through `IMidanTelemetrySink`, declared in Core. `MidanAI` never links
`MidanTelemetry`.

### A11 — Camera FOV conflict between the two documents

Master prompt §2.1 says "typically 75° → 100°". `ART_DIRECTION.md` §5 says base **72°**,
95–100° at top speed.

**Assumed:** `ART_DIRECTION.md` wins. It is the more specific document, its numbers are
derived from the actual reference frame, and it is the document you told me to prefer for
the budget. Same precedence applies to spring-arm length, pitch, lag values, look-ahead
yaw, and slip yaw.

### A12 — `MidanEditorTools` depending on three runtime modules

The master prompt says EditorTools "is editor-only and never referenced at runtime" but
does not enumerate what it may depend on. Its validators need the Data Asset types from
Vehicle, Race, and AI.

**Assumed:** permitted. It is an `Editor`-type module, absent from `Midan.Target.cs`, and
its absence from a Shipping binary is a hard gate in `verify_build.py` at Phase 10. The
dependency direction is inward only — no runtime module ever references EditorTools.

### A13 — Race distance is never specified

Sector count is fixed at 3 (§3.2) and the field is 8 cars (player + 7 AI). Lap count is
never stated anywhere.

**Assumed:** `LapCount` is a `UMidanRaceRulesDataAsset` field, defaulting to 3. Under the
no-hardcoded-tuning rule this cannot be a C++ constant regardless, so the default is a
starting point for you to tune, not a design decision I am making.

### A14 — Grid size

**Assumed:** 8 slots on `AMidanGridSpline` — player plus the seven AI opponents the
objective specifies.

### A15 — Two further referenced skills do not exist either

Beyond A2, the source documents reference two more skills that are not installed:

| Skill | Referenced by | Needed for |
|---|---|---|
| `defensible-data-analysis` | Master prompt §0.3 and §6.3 | Phase 9 — telemetry chart rigour, denominators, matched-condition comparison |
| `game-feel-engineering` | `docs/ART_DIRECTION.md` §5, line 149 | Phase 4 — the full camera behaviour list |

**Assumed for now:** not authored. Phase 4 leans on `docs/ART_DIRECTION.md` §5 plus master
prompt §2.1, which between them specify all seven camera behaviours concretely. Phase 9
leans on the reporting discipline captured in
`.claude/skills/realtime-performance-budget/SKILL.md` §4 and the `agent-eval-harness` skill.

**Say the word and I add both** — the gap is real, just not blocking until Phase 4.

### A20 — Two Core files pulled forward from Phase 3 to Phase 2

`MidanGameplayTags.h/.cpp` and `MidanLogChannels.h` were planned for Phase 3 but had
to land in Phase 2.

**Why:** `UVehicleSetupDataAsset::ValidateMidanData` must check that `VehicleClass`
descends from `Vehicle.Class`, and `CLAUDE.md` forbids constructing a tag from a string
literal at a call site — so the native tag had to exist before the rule that uses it could
be written correctly. Writing the rule with a string comparison and fixing it at Phase 3
would have meant shipping a known violation of the project's own convention.

`MidanLogChannels.h` followed because `UVehicleSetupApplier` refuses to apply an invalid
setup and must say why. Applying one anyway produces NaN in the solver, which surfaces as
the car silently vanishing — far harder to diagnose than a log line.

**Consequence:** Phase 3's file list shrinks by those two entries. No other phase is
affected, and the module dependency graph is unchanged.

### A21 — The Chaos field names in `VehicleSetupApplier.cpp` are unverified

Every Chaos assignment in that file is marked `// API VERIFY`. No engine is installed on
the build machine, so the field spellings were written from knowledge of the Chaos
Vehicles parameter surface rather than read from UE 5.8 headers.

The **structure** of the file and the **mapping decisions** are deliberate and reviewed —
what needs checking is the spelling. Three known version-sensitive spots, in order:

1. **Transmission shift timing.** Some versions expose a single `GearChangeTime` rather
   than separate up and down times, and some take absolute `ChangeUpRPM` rather than a
   ratio. The Data Asset stores shift points as a fraction of `MaxRPM` deliberately, so
   changing the rev limit does not silently move the shift points — that conversion stays
   regardless of which form Chaos wants.
2. **Centre-of-mass override field naming.**
3. **Write ordering — a correctness issue, not a spelling one.** Chaos snapshots wheel and
   engine configuration when the vehicle simulation is created, so `Apply` must run from
   the pawn's `PreInitializeComponents`, not `BeginPlay`. If a value visibly fails to take
   hold at runtime, check this before checking field names.

Verify against `Engine/Plugins/Runtime/ChaosVehicles/Source/ChaosVehicles/Public/`.

### A22 — Torque curves are authored in absolute Nm, not normalised

Chaos expects a torque curve normalised 0..1 scaled by a `MaxTorque` scalar. The Data
Asset stores **absolute Nm against RPM** and the applier converts.

**Why:** absolute Nm is the form a human can reason about and compare between the three
cars. Asking a designer to author a normalised curve plus a separate peak scalar splits
one decision across two fields and makes cross-vehicle comparison arithmetic.

**Consequence:** `GetPeakTorquePerTonne` reads the curve's own maximum, so it stays correct
without a second source of truth.

---

## Open — needs your decision before the phase noted

### A22 — Art direction pivoted to a neon night city · decided at Phase 3

**Found:** three reference images were supplied showing a wet neon night city in the
Need-for-Speed / GRID idiom, with a request to match them and to add day, night and deep-night
lighting. `ART_DIRECTION.md` v1.0 specified a golden-hour desert canyon with time-of-day
**locked to one condition** (§2.4), and the master prompt's non-goals list forbids
*"dynamic weather · day/night cycle"*.

**Resolved with the user:** full pivot. `ART_DIRECTION.md` v2.0 rewrites §2, §3 and §7 for a
wet neon city circuit with **three static presets** (Day, Night, Deep Night). Not a cycle —
that distinction is what keeps the §6 exposure clamp workable and the PSO surface finite, and
it keeps the master-prompt non-goal technically intact.

**Accepted costs, recorded so they are not rediscovered:**

| Cost | Consequence |
|---|---|
| 3× lighting authoring | Three complete light rigs plus three post presets, authored Deep Night first |
| ~3× PSO cache coverage | Master prompt §7.3's collection playthrough runs three times; shipped cache is the union. Skipping one ships a build that hitches on first selection of that preset |
| Environment bill of materials replaced | §3 canyon rock → modular city facade kit, signage library, urban furniture. Megascans cliff plan discarded |
| Budget re-allocated | See A23 |
| Day preset is now the VSM worst case | The budget must pass on all three presets, not just the hero one |

**What did NOT change:** §0 legal boundary, §4 vehicle art spec, §5 camera spec, §6 post
stack, §8 HUD spec, §9 screenshot discipline. The 16.6 ms total and the ≤ 15.0 ms GPU total
are untouched.

### A23 — The pivot is a net-zero budget re-allocation, not a budget increase

Night **removes** v1.0's largest performance risk: a −3° to −6° directional light casting
shadows across open terrain. Deep Night has no directional light at all. That frees 1.0 ms of
Virtual Shadow Map budget, which funds Lumen reflections (+0.5), a new local-lights/emissive
line (+0.75), and rain (+0.25), against volumetric clouds being cut (−0.5).

Sub-lines still sum to 14.0 ms with 1.0 ms contingency — arithmetically identical to v1.0.

**Assumed:** the re-allocation holds without a change to the 16.6 ms frame target, so
`CLAUDE.md`'s "changing the frame budget targets" stop condition is not tripped — the *total*
is unchanged and only its internal distribution moved. Flag this if you read it differently;
it is the one place where the pivot could be argued to need separate approval.

### A24 — 21st.dev and premium-frontend produce React, not UMG

**Found:** the request named 21st.dev MCP and the premium-frontend skill for the HUD. Both
target React/Motion/Three.js. Unreal's HUD is UMG/Slate. There is no direct transfer.

**Resolved with the user:** build a high-fidelity animated **web mockup as the authoritative
visual spec**, then translate to UMG C++ at Phase 7. Precedent exists in the repo —
`docs/midan_frontend_ui_screens_mockup.html` predates this decision.

**Consequence:** the mockup is a *specification artefact*, not shipped product. It must not
drift from `ART_DIRECTION.md` §8, which remains the authority on layout, bindings and
accessibility. Where the mockup and §8 disagree, §8 wins and the mockup is wrong.

### A25 — Vehicle art takes all three routes in parallel

**Found:** asked for "best generated cars". Claude Code cannot produce production vehicle
meshes — topology, separated wheel meshes with correct pivots, a bone rig, and real-world
metric scale are Blender work, budgeted at two weeks in `ART_DIRECTION.md` §4.3.

**Resolved with the user: all three routes**, which compose into a pipeline rather than
competing:

1. **Epic Vehicle Game sample mesh as a physics proxy** — the immediate path to a driveable
   car. Handling tuning can begin the moment the engine is installed, and the proxy swaps out
   later without touching code, because `UVehicleSetupApplier` reads the mesh from the Data
   Asset.
2. **Original concept art sheets** — the design authority the final model is built against.
   Generated, no real-marque geometry, badge, grille or light signature.
3. **Image-to-3D GLB blockouts** — visual placeholders only. Explicitly **not** production
   topology: expect fused wheels, no pivots, no rig, and wrong scale. Useful for framing
   screenshots and judging proportion, never for shipping.

**The legal boundary applies to all three.** A CC0 licence on a source mesh does not clear
trade dress, and an image generator asked for "a supercar" will happily produce something
nameable. Every generated design gets checked against §0 before it enters `Content/`.

### A26 — Three Core interfaces declared at Phase 3, not at their own phases

`docs/PHASE_PLAN.md` schedules `IMidanTrackInterface` and `IMidanRaceStateInterface` for
Phase 5 and `IMidanTelemetrySource`/`IMidanTelemetrySink` for Phase 9.
`UMidanServiceLocatorSubsystem` holds a `TScriptInterface` to each and cannot compile
against a forward declaration — `TScriptInterface<T>` needs the `UInterface` class to exist
for reflection.

**Assumed:** all three declared in `MidanCore` at Phase 3. They are Core's contracts
regardless of who implements them, so declaring them early costs nothing and changes no
dependency. Implementations still land at their planned phases: `AMidanTrackSpline` and
`AMidanRaceGameState` at Phase 5, `UMidanTelemetrySubsystem` at Phase 9.

Same precedent as A20 (gameplay tags pulled forward from Phase 3 to Phase 2).

### A27 — Async physics callback API not yet verified · Phase 3

`UVehicleAeroComponent` and `UVehicleSurfaceSensorComponent` both run in the async physics
callback via `AsyncPhysicsTickComponent`, enabled by `SetAsyncPhysicsTickEnabled(true)` in
`BeginPlay`. Master prompt §1.3 makes async physics non-negotiable and forbids force
application in `Tick`.

**Unverified, with no engine installed.** Marked `API VERIFY` at each site. The specific
spellings to confirm on 5.8:

| Call | Where |
|---|---|
| `SetAsyncPhysicsTickEnabled` / `AsyncPhysicsTickComponent(float, float)` | both async components |
| `FWheelStatus` fields — `bInContact`, `LongitudinalSlip`, `LateralSlip`, `SpringForce`, `NormalizedSuspensionLength`, `PhysMaterial` | surface sensor, assists, movement component |
| `GetWheelState(int32)` on `UChaosWheeledVehicleMovementComponent` | as above |
| `SetThrottleInput` / `SetBrakeInput` / `SetSteeringInput` / `SetHandbrakeInput` (bool vs float) | movement component |
| `IncreaseGear` / `DecreaseGear` vs `SetTargetGear` | movement component |
| `GetEngineRotationSpeed` / `GetCurrentGear` | pawn frame state |
| `AddForceAtLocation` force units and world-space location | aero component |

**The logic does not depend on the spellings.** A rename at first compile is a mechanical
fix; the force model, the assist ordering and the surface-lookup design are what the Phase 3
gate is actually asserting. Related: A3, A21.

### A28 — Vehicles are locked at four wheels

`MidanVehicleConstants::NumWheels = 4` is a `constexpr`, not a Data Asset field. Justified
under CLAUDE.md's exception because it is structural rather than tuning: the suspension and
tyre configs are authored per-axle, the surface sensor indexes a fixed array sized by it, and
Chaos wheel setups are built one-per-wheel at apply time. Supporting a different count is a
rewrite of all three, not a value change — and all three vehicles in
`docs/VEHICLE_SPEC.md` have four wheels.

### A29 — Two camera mode classes, not four · Phase 4

`docs/PHASE_PLAN.md` listed four camera mode files: ChaseFar, ChaseNear, Bonnet, Cockpit.

**Found:** there are only two BEHAVIOURS. ChaseFar and ChaseNear are a lagging boom
differing only in `FMidanCameraModeConfig` values; Bonnet and Cockpit are chassis-attached
and likewise differ only in data. Four classes would have been identical code with different
numbers.

**Assumed:** `FMidanBoomCameraMode` and `FMidanRigidCameraMode`. The mode COUNT is still
four and the player still cycles four; the variation lives entirely in the Data Asset, which
is the actual rule in CLAUDE.md rather than a deviation from it.

Both are plain C++ types with no `UWorld` dependency, so an Automation Spec can assert that
a 30-degree slip angle produces the authored slip yaw without spawning a car.

### A30 — Phase 4 moved nine values out of code into the Feel asset

A literal audit after the first Phase 4 pass found hardcoded values that were genuinely feel
tuning — the kind a human adjusts by ear or eye — which CLAUDE.md forbids in C++.

Moved to `UVehicleFeelDataAsset` / `FMidanCameraModeConfig`: `LookAheadDampingRate`,
`SlipYawDampingRate`, `VerticalDampingRateMax`/`Min`, `ShakeDecayRate`, `RumbleFadeRate`,
`FullScaleImpactImpulse`, `EngineLoadSmoothingRate`, `OverrunLoadWeight`,
`BackfireLoadThreshold`, `KerbStrikeIntensity`, `HapticSmoothingRate`,
`IdleRumbleFadeOutSpeedKmh`.

**Deliberately left as named constants**, each with a justification comment: the rumble
oscillator frequency ratios (they define "incommensurable", not a preference), the phase
wrap bound (float precision), `FullySidewaysAngleDegrees` (a denominator that would silently
rescale every authored value if changed), the shift-flag duration and backfire/kerb
cooldowns (event pacing, not character), and the minimum impact volume (a zero-volume
impact is a missing sound, which is a bug rather than a taste).

`FullScaleImpactImpulse` is deliberately SHARED by camera, audio and haptics. Three channels
disagreeing about how big the same collision was is worse than any one of them missing.

### A31 — Continuous force-feedback loops are declared but never spawned · Phase 4

`UVehicleHapticsComponent` declares `IdleRumble`, `SurfaceRumble` and `SlipRumble` and
modulates their intensity every frame, but never creates them.

**Reason:** the mechanism for modulating a LOOPING force feedback effect's amplitude at
runtime is the least certain API in the phase, and no engine is installed to check it
against. Spawning them against a guessed API would produce three silent components and a
false impression that the channel works.

Transient impacts and kerb strikes DO work — they go through `ClientPlayForceFeedback`,
which is a stable API.

**To close:** confirm the 5.8 looping-effect API, then spawn the three loops in
`InitialiseFromAsset`. The channel design does not change. Recorded in
`docs/MANUAL_STEPS.md` §4.8. Related: A27.

### A32 — Off-track detection is polled, not pushed · Phase 5

`docs/ARCHITECTURE.md` §2.3 lists `UMidanLapTimingSubsystem`'s cadence as "checkpoint
overlap events + off-track grace timer" — event-driven, not a per-frame `Tick`. But nothing
in the master prompt or the architecture doc says who calls the subsystem when a wheel
crosses onto gravel. `MidanVehicle` cannot push the event itself without depending on
`MidanRace`, which the module graph forbids (docs/ARCHITECTURE.md §2.1).

**Assumed:** `UMidanLapTimingSubsystem` polls every registered racer's
`IMidanVehicleInterface::GetVehicleFrameState` on a repeating `FTimerHandle`, at
`UMidanRaceRulesDataAsset::OffTrackPollIntervalSeconds` (default 0.1s, well under the
1.5s default grace period). This is a timer, not a `Tick` — it satisfies §2.3 literally —
and it keeps `MidanVehicle` and `MidanRace` decoupled: the subsystem reads through the same
interface `MidanTelemetry` uses, rather than `MidanVehicle` gaining a second consumer to
push to.

**Cost:** off-track state can lag up to one poll interval (100ms) behind the actual
substep transition. Acceptable against a 1.5s grace period; would need reconsidering if
the grace period is ever tuned below roughly 5× the poll interval — the validator warns
if it drops below 3×.

### A33 — `AMidanRaceGameMode` links `AIModule` for a placeholder `AAIController` · Phase 5

`AMidanOpponentController : AAIController` does not exist until Phase 6, but Phase 5's gate
needs a populated 8-car grid to exercise position calculation and the state machine. Master
prompt and `docs/ARCHITECTURE.md` §3.4 grant `AIModule` to `MidanAI` only.

**Assumed:** `MidanRace` links `AIModule` as a **private** dependency, for the sole purpose
of spawning a stock `AAIController` to possess opponent pawns (so they gain a
`PlayerState` and enter `PlayerArray`, per the `bWantsPlayerState` mechanism `APlayerState`
tracking relies on). This is an engine-module dependency, not a project-module one — it does
not touch the graph in docs/ARCHITECTURE.md §2.1, the same reasoning already applied to
`MidanAI`'s own `AIModule` link. Phase 6 replaces the controller class; this dependency
stays, since `AMidanOpponentController` itself derives from `AAIController`.

### A34 — Checkpoints are filtered by `IMidanVehicleInterface`, not by class · Phase 5

`AMidanCheckpoint`'s overlap trigger fires for anything physically inside its box. Filtering
by `Cast<AMidanVehiclePawn>` would pull `MidanVehicle` into `MidanRace`'s dependency list —
forbidden by the module graph. Filtering by `AActor::Implements<UMidanVehicleInterface>()`
instead costs one virtual dispatch and needs nothing beyond `MidanCore`, which `MidanRace`
already depends on.

### A35 — Respawn snaps to the last valid checkpoint, not the nearest track point · Phase 5

`docs/ARCHITECTURE.md` §3.3 specifies "rewind to last valid checkpoint" for
`MidanRespawnComponent`. The nearest point on the centreline to wherever a stuck racer
currently sits was considered and rejected: it can be the same hazard the racer just hit
(a wall apex, the inside of a hairpin a car has crashed into). `UMidanLapTimingSubsystem`
now tracks `LastValidCheckpointArcLength` per racer — the arc length of the last checkpoint
accepted into the sequence — specifically so the respawn component has a known-safe waypoint
to read, rather than reconstructing one from the racer's current (possibly the problem)
position.

### A36 — Overtake and avoidance detect other cars by world query, not by race data · Phase 6

`UMidanOvertakeComponent` and `UMidanAvoidanceComponent` both need to know "is there a car
near me and which way is it going" — but `MidanAI` depends on `MidanCore` and `MidanVehicle`
only, not `MidanRace`, so there is no position/lap registry to ask.

**Assumed:** both use direct world sphere-overlap/sweep queries (`UWorld::OverlapMultiByChannel`
/ `SweepSingleByChannel`) and filter hits by `AActor::Implements<UMidanVehicleInterface>()` —
the same interface-filtering pattern `AMidanCheckpoint` uses in `MidanRace` (A34). This
answers the proximity question with zero new dependencies: a physical trace does not care
which module owns lap timing.

**Cost:** an AI's avoidance/overtake logic cannot distinguish "car" from "any other actor
implementing the interface", which is exactly right — it should not need a special case for
who's driving. It also cannot read the OTHER car's intent (is it about to lane change too);
that is out of scope for Phase 6 and would need a shared coordination channel this phase does
not build.

### A37 — Rubber-band gap duplicates one line of MidanRace's math rather than depending on it · Phase 6

`UMidanRubberBandComponent::ComputeGapToPlayerSeconds` needs each racer's total race
progress (lap count × track length + arc length) to compare against the player's. That
formula already exists as `MidanRacePosition::ComputeTotalProgress` in `MidanRace`.

**Assumed:** duplicated inline as a one-line expression rather than depending on `MidanRace`
for it. A single arithmetic line is not worth a module edge that would need justifying at
every future architecture review; `IMidanTrackInterface` and `IMidanRaceStateInterface`
(both `MidanCore`) already supply every input the formula needs.

### A38 — `AMidanRaceGameMode` gains `OpponentControllerClass` to adopt `AMidanOpponentController` · Phase 6

Phase 5 possessed opponents with a hardcoded `AAIController::StaticClass()` and left a class
comment promising Phase 6 would replace it. `MidanRace` cannot reference
`AMidanOpponentController` by type without a `MidanRace → MidanAI` dependency edge, which is
not in the approved graph and is a stop condition to add.

**Assumed:** `AMidanRaceGameMode` gained one property, `TSubclassOf<AAIController>
OpponentControllerClass`, defaulting to `AAIController::StaticClass()` in the constructor.
`TSubclassOf<AAIController>` only needs `AIModule` (already a private dependency of
`MidanRace`, A33) — the *value* `AMidanOpponentController` is assigned in the GameMode
Blueprint, which is content configuration, not a new compiled dependency. This is the same
pattern as `DefaultPawnClass` and `OpponentVehicleClasses` already use for exactly this
reason.

### A39 — One shared reference speed profile per racing line, not one per difficulty tier · Phase 6

`docs/ARCHITECTURE.md` §3.4 lists `TyreFrictionMultiplier` as a difficulty field, and the
speed profile formula (`v = sqrt(mu*g/|curvature|)`) takes `mu` as an input — which could be
read as "regenerate the profile per difficulty tier with that tier's own `mu`".

**Assumed:** the racing line stores ONE reference profile, generated once at a reference
`mu` of 1.0. Each `AMidanOpponentController` instead applies `TargetSpeedMultiplier`
uniformly and `sqrt(TyreFrictionMultiplier)` specifically where
`FRacingLinePoint::bBrakingZone` is true, at runtime, per tick — two multiplies, not a
regeneration pass. `v ∝ sqrt(mu)` is why the multiplier is square-rooted rather than applied
flat, keeping the runtime approximation dimensionally consistent with what generated the
number it's scaling.

**Cost:** this is an approximation, not a re-derivation — a tier's actual achievable
cornering speed is not literally recomputed from its own `mu` against the corner's own
curvature, only scaled from the reference. Acceptable for a difficulty dial; would need
revisiting if a future tier's `mu` needs to be dramatically different from 1.0 (the
`ValidateMidanData` clamp keeps it in [0.3, 1.2] for exactly this reason).

### A40 — Vehicle-avoidance and overtake traces assume `ECC_Pawn` · Phase 6

`UMidanAvoidanceComponent` and `UMidanOvertakeComponent` both trace/sweep against
`ECC_Pawn`, assuming vehicle bodies and static track geometry both block on it.
**Unverified, no engine installed** — marked `API VERIFY` at each call site, same status as
every Chaos-adjacent assumption since Phase 3 (docs/ASSUMPTIONS.md A21, A27). Confirm
against the project's actual collision profile setup once the editor is available; if
vehicles use a dedicated `Vehicle` object channel instead, both components need the channel
constant updated together so they stay consistent with each other.

### A41 — Reaction delay is a fixed-capacity input ring buffer, not a slower Tick · Phase 6

`UMidanAIDifficultyDataAsset::ReactionDelaySeconds` needed a mechanism that delays the
AI's OUTPUT without delaying its READ of current state — a human driver's reaction time is a
lag between perceiving and acting, not a slower perception rate.

**Assumed:** `AMidanOpponentController` computes a fresh input every Tick (fresh
perception) and pushes it into a 256-sample ring buffer of `{timestamp, input}` pairs; each
Tick applies the newest buffered sample old enough to satisfy `ReactionDelaySeconds`. Fixed-
size C array, not `TArray` — zero allocation, same reasoning as
`MidanVehicleConstants::NumWheels` and the telemetry ring buffer. 256 samples covers up to a
1-second delay (the field's clamped maximum) at up to 256fps, which is headroom enough for
any realistic frame rate this project targets.

### A16 — Vehicle names chosen · RESOLVED, one check outstanding

`docs/VEHICLE_SPEC.md` previously read `## 1. [Name] — Hypercar` for all three cars.

**Resolved:** names delegated to me and chosen — **Sahm** (arrow, hypercar), **Raqs** (dance,
GT), **Hajar** (stone, rally). Arabic, matching the project name, each describing the car's
behaviour rather than decorating it. Rationale and the rejected wind-name space are recorded
in `docs/VEHICLE_SPEC.md` §Naming.

**Still open, and it is not something I can close:** no trademark search has been run. I
checked against manufacturers and models I know of and found no collision, which is not the
same thing as clearance. Before these names reach a shipped build or a portfolio page, run
each through a trademark register for automotive classes.

This keeps the legal boundary honest: the names are original as far as knowledge goes, and
unverified as far as registers go. Those are different claims and the docs state both.

### A17 — Canyon road vs closed 3 km loop · needed by Phase 5

`ART_DIRECTION.md` describes a canyon road with rock walls and a sun near the vanishing
point. The master prompt requires a 3 km **closed circuit** (§3.1, §5.2). Those are
compatible — a canyon circuit is fine — but §3.4 asks for **at least three** corner exits
that point the camera at the sun through a canyon gap, and the sun azimuth is locked to
one direction (§2.1).

On a closed loop, only the portions running roughly the same compass direction can achieve
that shot. Three such moments constrains the layout: the track needs three separate
stretches heading sunward, which means the loop is not a simple oval.

This is a track-layout decision for you at Phase 5, not a code decision. Flagged so it
informs the sculpt rather than being discovered after it.

### A18 — `bTickPhysicsAsync` with Chaos Vehicles · surfaces at Phase 3

Master prompt §1.3 mandates `bTickPhysicsAsync=True` with
`AsyncFixedTimeStepSize=0.008333`, and calls async physics tick plus substepping
"non-negotiable". Async physics combined with Chaos Vehicles has had version-specific
caveats around when wheel state is readable and which callback force application belongs
in.

**Plan:** configure exactly as specified at Phase 1. If it misbehaves on 5.8 / Metal, that
surfaces at the Phase 3 gate when `UVehicleAeroComponent` first applies force in the
callback. It would then be an **architecture fork with two valid paths** — async callback
versus substepped synchronous — which is a stop condition requiring your decision, not
something to work around quietly.

### A19 — PSO cache hitch numbers may be unobtainable on Mac · surfaces at Phase 10

Master prompt §7.3 requires measuring hitch count before and after PSO caching and putting
**both numbers in the README**. §7.3 step 3 uses `ShaderPipelineCacheTools Expand`, a
Windows-oriented toolchain path.

Mac-first (A4) means those two numbers may not exist until the Win64 lane does. Under the
measurement rule in `CLAUDE.md`, an unmeasured number cannot go in the README — so the
README would state the gap explicitly rather than omit the row silently.

Flagged now because "hitch count dropped from X to Y" is one of the three things the
master prompt identifies as what actually gets you hired (Part 2), and it is the one most
exposed by the Mac-first decision.

---

## Phase 7 assumptions

### A42 — `AMidanHUD.h` holds two classes, not one · Phase 7

`docs/PHASE_PLAN.md`'s Phase 7 file table names only `MidanHUD.h/.cpp` for the persistent
race panel (RPM, speed, gear, position, sector delta, assist/off-track flags, minimap) AND
the `AHUD` actor that owns every widget.

**Assumed:** both `AMidanHUD` (thin: create, own, show/hide, bind pause input) and
`UMidanHUDWidget` (the panel's live-data binding) live in the same file pair. The
alternative — inventing a `MidanHUDWidget.h/.cpp` the plan never named — seemed like a
larger deviation than two `UCLASS` in one header, which UHT supports without difficulty.

### A43 — Three `FMidanVehicleFrameState` fields added for the HUD · Phase 7

The HUD needs live TC/ABS intervention state and a normalised RPM value. Both are facts
only `MidanVehicle` classes hold — `UVehicleAssistComponent::GetActiveInterventions()` and
`UVehicleSetupDataAsset::Powertrain.MaxRPM` — and `MidanRace` does not depend on
`MidanVehicle` (docs/ASSUMPTIONS.md A7).

**Assumed:** `bTractionControlActive`, `bABSActive`, and `EngineRPMNormalised` added to
`FMidanVehicleFrameState` in `MidanCore`, populated by `AMidanVehiclePawn::GetVehicleFrameState`
(which already has both the assist component and the loaded setup asset), read by the HUD
through `IMidanVehicleInterface` like every other frame-state field. Same precedent as
`FMidanTrackPosition` and every other MidanCore POD: "changes when a system needs a field
it cannot derive."

**Cost:** `FMidanVehicleFrameState` grows by five bytes (two bools, one float) per instance,
written every frame in the game-thread `GetVehicleFrameState` call (not the async physics
callback, so the allocation-free rule does not apply — these are plain scalar writes).
Negligible against the struct's existing size.

### A44 — Sector-completion delta is a discrete broadcast payload, not a continuous read · Phase 7

ART_DIRECTION §8.1 calls for a "Live delta vs personal best sector." A continuously live
mid-sector delta would need the racer's historical pace at every point WITHIN a sector, not
just at its end — data this vertical slice does not collect (see docs/ASSUMPTIONS.md A9's
sibling reasoning: build what the spec needs, not a speculative superset).

**Assumed:** "live" is delivered as "updates the instant each sector completes," which is
what a racing HUD's sector delta conventionally shows anyway (compare any commercial
racing game's sector-time readout: it appears at the split, not continuously). Implemented
by computing the delta inside `UMidanLapTimingSubsystem::HandleCheckpointCrossed`, before
the subsystem's own best-sector record updates (a new best would otherwise overwrite the
value being compared against), and passing it as a third `FOnMidanSectorCompleted`
parameter rather than leaving `AMidanRacePlayerState` to recompute it from data that may no
longer be available by the time the broadcast is handled.

### A45 — UMG widget `Tick` is added to the pre-committed tick inventory · Phase 7

`docs/ARCHITECTURE.md` §2.3 requires every ticking system to be pre-committed and
justified; additions need justification. A HUD showing live speed, RPM, and position is
inherently a per-frame concern — there is no meaningful "timer instead" for rendering
current state to the screen every frame.

**Assumed:** `UMidanHUDWidget` and the state-driven widgets tick via UMG's standard
`NativeTick`, added to the §2.3 table under the same justification already granted to
`UMidanChaseCameraComponent` ("a render-rate concern by definition"). This is the
established, idiomatic mechanism for live UI in Unreal; building a custom timer-driven
alternative would not reduce cost — the work still happens every rendered frame regardless
of which mechanism triggers it — and would only add a bespoke pattern for no benefit.
