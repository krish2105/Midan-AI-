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

### A16 — Vehicle names are placeholders · needed by Phase 2

`docs/VEHICLE_SPEC.md` reads `## 1. [Name] — Hypercar` for all three cars. Three original
names are required, and this is a legal boundary rather than a naming preference.

Candidates will be proposed at the Phase 2 gate. You approve or replace them.

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
