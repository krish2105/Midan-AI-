# MIDAN — UE5 RACING VERTICAL SLICE
## Claude Code Master Build Prompt

**ميدان (midan) · "the arena / the field"**
Version 1.0 · Target: 12 weeks · UE 5.5+ · Windows shipping build · Zero paid tooling

---

# PART 0 — PRE-FLIGHT (read this, do not paste it)

## 0.1 The hard boundary you must understand before starting

**Claude Code cannot drive the Unreal Editor.** It cannot click, sculpt a landscape, rig a mesh, place an actor, or tune a value by feel. Any prompt that pretends otherwise will waste your time.

What it *can* do is roughly 70% of the actual engineering work:

| Claude Code does | You do in the editor / Blender |
|---|---|
| All C++ (modules, classes, components, subsystems) | Landscape sculpting, mesh placement |
| `.Build.cs`, `.Target.cs`, module wiring | Vehicle rigging in Blender |
| Data Asset class definitions + schemas | Filling Data Asset values, feel tuning |
| Python editor-automation scripts (UE Python API) | Material graph authoring |
| Racing-line, AI controller, lap-timing systems | Spline placement, checkpoint layout |
| Telemetry capture + Python analysis tooling | Playtesting, judging feel |
| Build/cook/package automation, CI, PSO scripts | Final art pass, audio recording |
| Automation Spec tests, Gauntlet harness | The 40% that is taste |

**Design consequence:** this project is architected so that *everything mechanical lives in C++ and data*, and *everything requiring taste lives in Data Assets and Blueprints you tune by hand*. That split is not a workaround — it is how professional teams build racing games.

## 0.2 Model per phase

| Phase | Model | Why |
|---|---|---|
| 0 (architecture, module plan) | **Opus 5**, Plan Mode | Module boundaries and the data architecture are expensive to get wrong. |
| 1–3 (scaffold, vehicle data, vehicle core) | **Sonnet 5** | Spec locked, heavy C++ throughput. |
| 4 (feel layer: camera, audio, FX) | **Opus 5** | Feel is taste-adjacent even in code. Curve shapes matter. |
| 5–7 (track, AI, race flow) | **Sonnet 5** | Algorithmic, well-specified. |
| 8 (rendering & performance) | **Opus 5** | Budget allocation and scalability strategy need judgement. |
| 9–10 (telemetry, build, CI, ship) | **Sonnet 5** → **Haiku 4.5** for docs | Mechanical. |

## 0.3 Skills

You have: `grounded-agent-architecture`, `defensible-data-analysis` (for the telemetry phase), `agent-eval-harness` (for the automated test harness), `frontend-design` (only if you build a web results page).

None of your existing skills cover UE5 architecture. **Ask Claude to write `unreal-cpp-architecture` as its first Phase 0 deliverable** if you want it reusable — otherwise the conventions live in `CLAUDE.md`.

## 0.4 Machine and account prerequisites (do these before Phase 0)

- **Disk:** 250 GB free minimum. UE5 + DDC + intermediate + builds is enormous.
- **RAM:** 32 GB strongly recommended. 16 GB will compile but shader compilation will hurt.
- **GPU:** anything with hardware ray tracing helps but is not required (software Lumen path is specified below).
- **Epic Games account** → install **UE 5.5 or newer** + **Visual Studio 2022** with "Game development with C++" workload (Windows) or Xcode (Mac — note: Mac shipping builds for a racing slice are viable but Lumen/Nanite performance is materially worse; if you are on the M4 Pro, plan to develop on Mac and produce the Windows shipping build on a Windows machine or via a cloud runner).
- **Blender 4.x** (free)
- **Git + Git LFS** — install and configure LFS **before your first asset commit**. Retrofitting is painful.
- **FMOD Studio** free indie tier (optional; MetaSounds path is specified as the default so you have zero external dependency)
- Free asset sources: Quixel Megascans (free in UE), Epic Vehicle Game sample, City Sample, Sketchfab CC0, freesound.org CC0

## 0.5 What to attach

1. This file, at repo root.
2. `docs/VEHICLE_SPEC.md` — you write ~30 lines by hand describing the three cars you want (class, drivetrain, mass, power band character, intended handling personality). Template in §3.5.
3. Nothing else.

**⚠️ Read the Scope Locks and Stop Conditions in Part 1 before pasting.**

---

---

# PART 1 — THE MASTER PROMPT (paste everything below this line)

---

## Objective

Act as a **senior game engineer**. Build **Midan** — a production-quality racing vertical slice in Unreal Engine 5: one 3km circuit, three drivable vehicles with distinct handling personalities, seven AI opponents, full race flow, telemetry capture, a profiling harness, and an automated build-cook-package pipeline producing a Windows shipping executable.

This is a **portfolio vertical slice**, not a game. Four minutes that feel expensive beat four hours that feel cheap. Depth over breadth is the explicit strategy.

**You cannot use the Unreal Editor.** Every deliverable must be C++, `.Build.cs`/`.Target.cs`, Data Asset class definitions, Python editor-automation scripts using the UE Python API, config `.ini`, shell/Python build automation, or documentation. At every phase gate, output a **`MANUAL_STEPS.md` section** listing exactly what the human must do in the editor or Blender, with precise field names and values.

---

## Context — LOCKED architectural decisions, do not relitigate

### The C++ / Data / Blueprint split (the most important rule in this repo)

| Lives in | Contains | Never contains |
|---|---|---|
| **C++** | Systems, algorithms, components, subsystems, interfaces, math | Tuning numbers |
| **Data Assets** | Every tuning value: torque curves, friction, camera curves, AI difficulty | Logic |
| **Blueprint** | Thin subclasses of C++ classes, asset references, cosmetic FX wiring | Systems, math, gameplay rules |

**Rationale:** a designer (you, tuning by feel) must be able to change how a car drives without recompiling. A programmer must be able to change how the physics is applied without touching a Blueprint graph. If a value is something a human would tune by feel, it belongs in a Data Asset — full stop.

**Corollary rule:** no gameplay-relevant hardcoded float in C++. Every constant is either a `UPROPERTY(EditDefaultsOnly)` on a Data Asset or a named constant with a comment explaining why it can never change.

### Module structure — build exactly this

```
Source/
├── Midan.Target.cs
├── MidanEditor.Target.cs
├── MidanCore/          # framework, subsystems, interfaces, shared types
├── MidanVehicle/       # vehicle pawn, physics config, feel systems
├── MidanRace/          # track, checkpoints, lap timing, race flow
├── MidanAI/            # racing line, opponent controller
├── MidanTelemetry/     # capture, serialisation, ghost replay
└── MidanEditorTools/   # editor-only: validators, spline tools (WITH_EDITOR)
```

Rules: no circular dependencies. `MidanCore` depends on nothing. `MidanVehicle` may depend on Core. `MidanAI` depends on Core + Vehicle. `MidanEditorTools` is editor-only and never referenced at runtime.

### Engine features — locked

- **Chaos Vehicles** (`ChaosVehiclesPlugin`) — the vehicle physics solver
- **Enhanced Input** — not legacy input bindings
- **World Partition** + HLOD for the 3km track
- **Nanite** for static geometry; **Lumen** (software tracing default, hardware optional) for GI + reflections
- **Virtual Shadow Maps**
- **TSR** (Temporal Super Resolution) — not TAA, not FSR, for the default upscaler
- **MetaSounds** for audio — zero external dependencies
- **PCG** for foliage scatter
- **Gameplay Tags** for state, never string comparisons
- **Async physics tick + substepping** — non-negotiable for vehicle stability

### Non-goals for v1 — do NOT build these

Open world · car customisation/upgrades · multiplayer or networking · damage modelling · dynamic weather · day/night cycle · more than one track · licensed vehicles or real brand names · console platforms · monetisation.

---

## Scope Locks

**Forbidden actions:**
- NEVER write gameplay logic in Blueprint. Blueprints are thin data-carrying subclasses only.
- NEVER hardcode a tuning value in C++.
- NEVER use `Tick` where a timer, event, or the physics callback will do. State every per-frame cost you introduce.
- NEVER allocate in a physics callback or per-frame hot path.
- NEVER use `FindObject`, `LoadObject`, or hard references in constructors. Use `TSoftObjectPtr` and async loading.
- NEVER commit `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`.
- NEVER commit a binary asset without Git LFS configured for its extension.
- NEVER reference a real car manufacturer, model name, or logo. Vehicles are original fictional designs. This is a legal boundary, not a stylistic one.
- NEVER use an asset without recording its licence in `docs/ASSET_LICENCES.md`.
- NEVER add a plugin or third-party dependency without asking.
- NEVER `git push`, tag a release, or upload a build without asking.
- NEVER claim a performance number you have not measured.

---

## Repository Structure

```
midan/
├── CLAUDE.md
├── README.md
├── .gitignore
├── .gitattributes                    # Git LFS rules
├── Midan.uproject
├── Config/
│   ├── DefaultEngine.ini
│   ├── DefaultGame.ini
│   ├── DefaultInput.ini
│   └── DefaultScalability.ini
├── Source/                           # (module layout above)
├── Content/                          # editor-authored, LFS-tracked
├── Tools/
│   ├── editor_python/                # UE Python automation scripts
│   │   ├── validate_vehicle_data.py
│   │   ├── generate_racing_line.py
│   │   ├── batch_setup_checkpoints.py
│   │   └── capture_perf_baseline.py
│   ├── build/
│   │   ├── build.py                  # wraps UAT BuildCookRun
│   │   ├── package_shipping.py
│   │   ├── generate_pso_cache.py
│   │   └── verify_build.py
│   └── analysis/
│       ├── telemetry_report.py       # lap telemetry → charts
│       └── perf_report.py            # Insights trace → budget table
├── docs/
│   ├── VEHICLE_SPEC.md               # ATTACHED BY USER
│   ├── ARCHITECTURE.md
│   ├── PERFORMANCE_BUDGET.md
│   ├── MANUAL_STEPS.md               # grows every phase
│   ├── ASSET_LICENCES.md
│   └── BUILD_RUNBOOK.md
└── .github/workflows/                # CI (self-hosted runner)
```

---

## §1 — VEHICLE PHYSICS ARCHITECTURE (Chaos)

### 1.1 The data architecture

Do **not** configure vehicles by hand-editing a Blueprint's Chaos component. Build a data pipeline:

```cpp
UCLASS(BlueprintType)
class MIDANVEHICLE_API UVehicleSetupDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Category="Identity")
    FText DisplayName;                       // fictional only

    UPROPERTY(EditDefaultsOnly, Category="Identity")
    FGameplayTag VehicleClass;               // Vehicle.Class.Hyper / GT / Rally

    UPROPERTY(EditDefaultsOnly, Category="Mass")
    FVehicleMassConfig Mass;                 // kg, COM offset, inertia scale

    UPROPERTY(EditDefaultsOnly, Category="Powertrain")
    FVehiclePowertrainConfig Powertrain;     // torque curve, gears, final drive

    UPROPERTY(EditDefaultsOnly, Category="Drivetrain")
    FVehicleDrivetrainConfig Drivetrain;     // FWD/RWD/AWD, torque split, diff

    UPROPERTY(EditDefaultsOnly, Category="Suspension")
    FVehicleSuspensionConfig Suspension;     // per-axle: travel, spring, damping

    UPROPERTY(EditDefaultsOnly, Category="Tyres")
    FVehicleTyreConfig Tyres;                // per-axle friction, surface curves

    UPROPERTY(EditDefaultsOnly, Category="Steering")
    FVehicleSteeringConfig Steering;         // speed-sensitive curve, rate limits

    UPROPERTY(EditDefaultsOnly, Category="Aero")
    FVehicleAeroConfig Aero;                 // drag, downforce coefficients

    UPROPERTY(EditDefaultsOnly, Category="Feel")
    TObjectPtr<UVehicleFeelDataAsset> Feel;  // camera, audio, FX — see §2

    UPROPERTY(EditDefaultsOnly, Category="Assists")
    FVehicleAssistConfig Assists;            // TC, ABS, stability, steering assist
};
```

A `UVehicleSetupApplier` (C++, called on `BeginPlay` and on a `#if WITH_EDITOR` hot-reload delegate) writes these values into `UChaosWheeledVehicleMovementComponent` and the wheel setups. **The Data Asset is the source of truth; the Chaos component is a downstream consumer.**

Why this matters: it makes vehicles diffable in version control, validatable in CI, tunable without recompiles, and — critically for you — **generatable and sweepable**, which is what makes the telemetry phase possible.

### 1.2 Chaos parameters that actually matter, and what they do

Implement every one of these as a Data Asset field with a doc comment explaining the *handling consequence*, not just the units.

**Mass and inertia**
| Field | Handling consequence |
|---|---|
| `Mass` (kg) | Everything. Set realistically first, tune elsewhere. |
| `CentreOfMassOffset` (Z) | **A design dial, not a bug fix.** Lowering it artificially stabilises the car and kills body movement. For a rally car that removes the character you want. Expose it; never quietly lower it to fix a flip. |
| `InertiaTensorScale` | Rotational willingness. Raise for reluctance, lower for agility. |

**Powertrain**
| Field | Consequence |
|---|---|
| `TorqueCurve` (`UCurveFloat`, Nm vs RPM) | The difference between *fast* and *feels fast*. Peaky curves feel dramatic; flat curves feel fast but dull. |
| `MaxRPM`, `EngineIdleRPM`, `EngineBrakeEffect` | Off-throttle deceleration is a huge feel contributor. |
| `GearRatios[]`, `FinalRatio` | Gear spacing controls how often the player feels an event. |
| `ChangeUpTime` / `ChangeDownTime` | Shift punch. Sub-0.15s reads as a modern dual-clutch. |

**Drivetrain**
`DifferentialType` (AllWheelDrive / FrontWheelDrive / RearWheelDrive), `FrontRearSplit`, plus front/rear left-right splits. RWD + high torque = oversteer character; AWD = stability and traction off the line.

**Suspension (per axle)**
`SuspensionMaxRaise`, `SuspensionMaxDrop`, `SpringRate`, `SpringPreload`, `SuspensionDampingRatio`, `SuspensionForceOffset`, `WheelLoadRatio`.
**Body roll is how the player reads grip.** Over-damping produces a car that is fast and feels dead. Under-damping produces a boat. This is where most amateur racing games fail.

**Tyres (per axle)**
`FrictionForceMultiplier`, `LateralSlipGraph` (`UCurveFloat`), `CorneringStiffness`, `SlipThreshold`, `SkidThreshold`.
**The front/rear friction ratio is your understeer/oversteer balance.** Higher rear than front = understeer (safe, dull). Higher front than rear = oversteer (exciting, punishing). Expose the ratio as a single derived tuning value in the Data Asset so it can be swept.

**Steering**
`SteeringCurve` (`UCurveFloat`: max steer angle vs forward speed) — **mandatory**. Never map raw stick/key input directly to steer angle. Also: `SteeringInputRate` (rise/fall interpolation), `AckermannAccuracy`.
For keyboard input, implement a separate input-shaping curve — binary keys through a linear map is the single most common reason a UE5 car "feels wrong".

**Aero**
Chaos does not give you downforce out of the box in a usable form. Implement a `UVehicleAeroComponent` that, in the physics callback, applies:
- Drag: `-0.5 * ρ * Cd * A * v² * v̂` at centre of pressure
- Downforce: `-0.5 * ρ * Cl * A * v²` applied at separate front/rear application points

Front/rear downforce balance shifting with speed is what makes a high-speed car feel planted and a low-speed car feel nervous. This is a **genuinely senior touch** and cheap to implement.

**Surfaces**
Physical Materials (`Tarmac`, `Kerb`, `Gravel`, `Grass`, `Sand`, `Wet`) each map to a friction multiplier and an audio/FX row in a Data Table. Off-track detection reads the physical material under each wheel — do not use trigger volumes for this.

### 1.3 Physics stability (do this in Phase 3 or you will fight it forever)

```ini
[/Script/Engine.PhysicsSettings]
bSubstepping=True
MaxSubstepDeltaTime=0.008333        ; 120Hz physics
MaxSubsteps=6
bTickPhysicsAsync=True
AsyncFixedTimeStepSize=0.008333
```

Rules:
- All vehicle force application happens in the **async physics callback**, never in `Tick`.
- Chassis collision: **convex hull**, not complex. Wheels: sphere or capsule.
- Validate the physics asset weights the bones correctly — bad bone weighting is the #1 cause of "flips like a Beyblade".
- Never scale a vehicle actor at runtime. Chaos does not like it.

---

## §2 — THE FEEL LAYER (40% of the experience, 10% of the code)

Ship this as `UVehicleFeelDataAsset` + a set of C++ components. Every value is a curve, not a constant.

### 2.1 Camera — `UMidanChaseCameraComponent`

| Behaviour | Implementation |
|---|---|
| Speed FOV | `FOV = BaseFOV + FOVCurve.Eval(speed01)`. Typically 75° → 100°. **The single biggest speed-sensation lever.** |
| Positional lag | Spring-arm lag with separate location/rotation speeds, curve-driven by speed |
| Look-ahead | Yaw offset from steering input, damped — the camera anticipates the corner |
| Slip yaw | Additional yaw from chassis slip angle so drifts read clearly |
| Vertical damping | Suppress suspension chatter reaching the camera; keep big impacts |
| Impact shake | Camera shake scaled by impact impulse magnitude, curve-mapped |
| Surface rumble | Low-amplitude noise driven by surface roughness per wheel |

Provide four camera modes (chase far, chase near, bonnet, cockpit) behind one interface.

### 2.2 Audio — MetaSounds, no external dependency

- **Engine:** minimum 4 RPM sample layers (idle, low, mid, high) crossfaded on RPM, with **separate on-load and off-load sets** crossfaded by throttle. Two-axis blending is what separates convincing engine audio from a whine.
- **Transmission:** shift-up punch, shift-down blip, driven by gear-change events.
- **Tyres:** slip-driven scrub, surface-dependent, per-axle so oversteer sounds different from understeer.
- **Wind:** speed-driven noise layer.
- **Impacts:** impulse-magnitude-mapped.
- **Doppler + attenuation** on AI vehicles; ensure the player's own vehicle bypasses Doppler.

Wire a `UVehicleAudioComponent` that reads physics state each frame and pushes parameters into the MetaSound. Keep the sound design in the MetaSound graph (human-authored); keep the parameter mapping in C++.

### 2.3 Visual feedback
Speed-scaled motion blur intensity · subtle chromatic aberration above a speed threshold · radial vignette curve · tyre smoke particle rate driven by slip · surface-dependent particle spawning (dust, gravel, water spray) · skid decal deposition gated by slip threshold with a pooled decal budget · exhaust backfire on overrun.

### 2.4 Haptics
Force feedback curves for: engine idle rumble, surface roughness, wheel slip, impacts, kerb strikes. `UForceFeedbackEffect` assets referenced from the Feel Data Asset.

---

## §3 — TRACK & RACE SYSTEMS

### 3.1 Track construction pipeline
- Road built from a **spline + spline mesh components** (`AMidanTrackSpline`), not hand-placed meshes. Exposes width, banking, and a material-per-section array.
- An editor Python tool (`Tools/editor_python/`) generates checkpoints at even arc-length intervals along the spline and names them deterministically.
- Landscape sculpted by hand (manual step), foliage scattered via PCG driven by a distance-from-spline exclusion mask.
- World Partition with a 3km loop; HLOD levels generated via commandlet, scripted.

### 3.2 Lap and sector timing — `UMidanLapTimingSubsystem`
- Checkpoint sequence validation (prevents corner-cutting and reverse-direction exploits)
- Sector splits (3 sectors), best-sector tracking, theoretical-best lap
- Off-track detection via physical material sampling under wheels, with a grace-time threshold before a lap is invalidated
- Rewind/respawn to last valid checkpoint with velocity and rotation restoration, on a cooldown

### 3.3 Race flow
`AMidanRaceGameMode` + `AMidanRaceGameState` + `AMidanRacePlayerState`. States as Gameplay Tags: `Race.State.Grid` → `Countdown` → `Racing` → `Finished` → `Results`. Grid positions from a data-driven grid spline. Position calculation from arc-length-along-spline + lap count, updated at 10Hz, not per frame.

---

## §4 — AI OPPONENTS

Do **not** use Behaviour Trees for racing AI. Racing is a continuous control problem, not a discrete decision problem.

### 4.1 Racing line
A separate spline from the road centre, authored by hand or generated. Each spline point carries: target speed, braking-zone flag, allowed lateral offset range. Provide an editor Python tool that computes an initial speed profile from **curvature lookahead**:

```
v_target(s) = min( v_max,
                   sqrt(µ_effective * g / |curvature(s)|),
                   backward_pass_braking_limit(s) )
```

Run a backward pass from each corner apex to compute braking points from deceleration capability. This produces a genuinely good first draft that a human then adjusts.

### 4.2 Path following — `UMidanRacingLineFollower`
- **Lateral control:** pure-pursuit with a speed-dependent lookahead distance, or a Stanley controller. Implement pure-pursuit first; it is more forgiving.
- **Longitudinal control:** PID on the difference between current speed and target speed at the lookahead point, with separate throttle and brake gains and an anti-windup clamp.
- Output **only normalised throttle/brake/steer in [-1,1]**, fed into the exact same vehicle input path as the player. The AI must not cheat physics. This is a hard architectural rule.

### 4.3 Racing behaviour
- **Overtaking:** lateral offset lanes on the racing line. An opponent evaluates a lane change when closing speed and gap thresholds are met and the target lane is clear on a predictive sweep.
- **Avoidance:** predictive sphere traces along the projected path 1.5s ahead; blend a lateral avoidance offset rather than braking, wherever possible.
- **Defence:** the lead car may take a defensive offset once per straight — subtle, never blocking.
- **Mistakes:** a small probability of a late brake or wide line, scaled inversely with difficulty. Perfect AI is boring AI.

### 4.4 Difficulty — the honest way
Difficulty modifies: tyre friction multiplier, reaction delay, target-speed multiplier, mistake probability, and aggression. It **never** teleports, never adds engine power beyond the player's car class, and rubber-banding is limited to a small speed-multiplier envelope that decays to 1.0 within a few seconds. Log every rubber-band application to telemetry so you can prove it is subtle.

---

## §5 — RENDERING & PERFORMANCE (senior-level, budget-driven)

### 5.1 Frame budget — write this into `docs/PERFORMANCE_BUDGET.md` and hold to it

Target: **60 fps at 1440p on a mid-range GPU**, with a 120 fps scalability path.

| Budget line | Target (ms) |
|---|---|
| Total frame | 16.6 |
| Game thread | ≤ 6.0 |
| Render thread | ≤ 6.0 |
| RHI thread | ≤ 4.0 |
| GPU total | ≤ 15.0 |
| — Base pass (Nanite) | ≤ 3.5 |
| — Lumen GI + reflections | ≤ 4.0 |
| — Virtual Shadow Maps | ≤ 2.5 |
| — Post + TSR | ≤ 2.5 |
| — Translucency/particles | ≤ 1.5 |
| Draw calls | ≤ 3,000 |

**Every phase gate after Phase 3 must report measured numbers against this table.** A number you have not measured does not go in the table.

### 5.2 Rendering configuration — locked

**Nanite:** all static track geometry, barriers, buildings, rocks. **Not** vehicles (skeletal), not translucent materials, not foliage that needs wind vertex animation unless you are on 5.5+ and have measured it.

**Lumen:** software tracing as the default (broad hardware compatibility, better for a 3km streamed track). Hardware ray tracing as an optional quality tier. Set `Lumen Scene Detail` and `Final Gather Quality` per scalability level, not globally.

**Virtual Shadow Maps** on. Watch the page-pool cost with a large directional light on an open track — this is a common blowout. Clamp `r.Shadow.Virtual.ResolutionLodBiasDirectional`.

**TSR** as the upscaler. For a racing game specifically: TSR's history rejection handles high-velocity screen movement better than TAA. Tune `r.TSR.History.ScreenPercentage` and verify no ghosting on wheels and barriers at speed — **wheel ghosting is the classic racing-game artefact** and a reviewer will notice it instantly.

**Motion blur:** object motion blur **on**, camera motion blur low. This is a speed-feel system as much as a rendering one. Expose intensity in settings — some players hate it.

**Streaming:** World Partition with a runtime grid sized to the track; HLOD levels 0 and 1 generated by commandlet. Cell size tuned so the streaming radius fits comfortably ahead of top speed — a 3km track at 300km/h covers 83m/s, so your streaming distance must exceed 250m of travel-time headroom.

**Materials:** shared master material with instance parameters. Hard limit on unique master materials (target ≤ 12) to control shader permutation count and PSO cache size.

### 5.3 Scalability
Author `DefaultScalability.ini` with four tiers. Provide an in-game settings menu backed by `UGameUserSettings` subclass, with a benchmark-driven auto-detect on first run.

### 5.4 Profiling harness (C++ + Python, automatable)
- `stat unit`, `stat gpu`, `stat scenerendering` capture hooked to a console command
- **Unreal Insights** trace capture on a scripted hot-lap replay
- `Tools/analysis/perf_report.py` parses the trace and emits the budget table with pass/fail per line
- A **deterministic hot-lap replay** (from recorded telemetry inputs) so performance runs are comparable across builds — this is what makes the numbers trustworthy

---

## §6 — TELEMETRY, GHOSTS & YOUR ACTUAL EDGE

This section is where your ML background differentiates the project from every other UE5 racing portfolio.

### 6.1 Capture — `UMidanTelemetrySubsystem`
At a fixed 60Hz (decoupled from frame rate), record per vehicle: timestamp · position · rotation · velocity · per-wheel slip ratio and slip angle · per-wheel load · per-wheel surface material · throttle/brake/steer/handbrake · gear · RPM · lateral and longitudinal G · distance along racing line · lap and sector.

Write to a ring buffer, flush to disk asynchronously. **Never allocate or do file I/O on the game thread.** Serialise as compact binary; provide a CSV export path.

### 6.2 Ghost replay
Replay from recorded **inputs plus periodic state resync**, not from raw transforms. Input replay demonstrates deterministic physics; transform replay is a cheap trick and an interviewer will ask which you did.

### 6.3 Analysis tooling — `Tools/analysis/telemetry_report.py`
Generate: speed trace vs distance, throttle/brake overlay, racing-line deviation heat map, slip-angle distribution per corner, sector-time consistency, and a **player-vs-AI delta trace** showing exactly where time is lost.

Apply the `defensible-data-analysis` skill here: every chart carries its sample count, laps are only compared within matched conditions, and no metric is presented without its denominator.

### 6.4 Optional ML extension (only after the slice ships)
With telemetry in hand: a corner-speed model predicting achievable entry speed from curvature and surface; a lap-time predictor; or an imitation-learned AI trained on your own recorded laps to compare against the hand-authored controller. **Do not start this before the slice is playable** — it is a follow-up project, and saying so demonstrates judgement.

---

## §7 — BUILD, PACKAGE & DEPLOY PIPELINE (the "C-level" ship discipline)

### 7.1 Build configurations
| Config | Purpose |
|---|---|
| `DebugGame Editor` | Debugging C++ in editor |
| `Development Editor` | Daily work |
| `Development` | Standalone with stats and console |
| `Test` | Shipping-like + stats + Insights (**profile against this, not Development**) |
| `Shipping` | Final. No console, no stats, full optimisation. |

### 7.2 Automated build — `Tools/build/build.py`
Wrap UAT `BuildCookRun`. Parameters to expose: platform, configuration, cook-on-the-fly vs cooked, pak, IoStore, compression, staging directory, archive directory. Emit a build manifest with git SHA, engine version, timestamp, and content hash.

```
RunUAT BuildCookRun
  -project=<abs>/Midan.uproject
  -noP4 -platform=Win64 -clientconfig=Shipping
  -cook -allmaps -build -stage -pak -iostore -compressed
  -archive -archivedirectory=<abs>/Builds
```

### 7.3 PSO caching — do not skip this
Shader hitching is the most common reason an indie racing build feels broken while the editor feels fine.

1. Enable `r.ShaderPipelineCache.Enabled=1` and logging in a Test build
2. Run a scripted playthrough covering every vehicle, every camera mode, every surface, and both weather-free lighting states
3. Collect the `.rec.upipelinecache`, run `ShaderPipelineCacheTools Expand` to produce a stable cache
4. Ship the bundled cache with the Shipping build
5. **Measure hitch counts before and after and put both numbers in the README**

Automate steps 1–4 in `Tools/build/generate_pso_cache.py`.

### 7.4 Automated verification — `Tools/build/verify_build.py`
Post-package checks that fail the build: executable exists and launches to main menu within N seconds (headless smoke test) · pak file integrity · build size within budget · no `Development`-only content cooked · manifest matches git SHA · no editor-only module referenced.

### 7.5 Automated tests
- **Automation Spec** (C++) for: lap validation logic, checkpoint sequencing, racing-line speed-profile math, PID convergence, telemetry serialisation round-trip, Data Asset validation rules.
- **Functional Tests** (map-based) for: vehicle spawns and drives, AI completes a lap without leaving track, respawn restores valid state, race completes and produces results.
- **Gauntlet** harness for the deterministic hot-lap performance run.
- Run all of it from `RunUAT RunUnreal` in CI.

### 7.6 CI — GitHub Actions with a self-hosted runner
Hosted runners cannot build UE5 (disk and licence constraints). Configure a self-hosted Windows runner. Pipeline: compile all modules → run Automation Specs → cook a Development build → run Functional Tests → on tag, produce a Shipping build → run `verify_build.py` → upload artefact.

Keep DDC shared and warm between runs; cold DDC turns a 10-minute build into 90.

### 7.7 Distribution
- **itch.io** — free, no gatekeeper, correct for a portfolio. Use Butler for scripted uploads and channel-based versioning.
- **GitHub Releases** for the versioned binary + build manifest.
- **Steam** requires a per-title fee and a review process — out of scope for v1; note it in the runbook as a future step.
- Ship with: a README, controls card, minimum specs, known issues, and asset licence attributions.

### 7.8 Release runbook — `docs/BUILD_RUNBOOK.md`
Version bump → full test pass → PSO cache regeneration → Shipping package → verify script → performance run against budget table → smoke test on a clean machine → tag → upload → release notes. Written as a checklist someone else could follow.

---

## §8 — BUILD PHASES (STOP at every gate)

Every gate must output: `✅` summary, files created, **measured** numbers where applicable, and an updated `docs/MANUAL_STEPS.md` section with exact editor instructions for the human.

### Phase 0 — Architecture *(Opus 5, Plan Mode)*
Read this prompt and `docs/VEHICLE_SPEC.md`. Produce: `CLAUDE.md` (locked stack, the C++/Data/Blueprint split, forbidden actions, the "no hardcoded tuning value" rule), `docs/ARCHITECTURE.md` with the module dependency graph and class inventory, `docs/PERFORMANCE_BUDGET.md`, and a file-level plan for Phases 1–10. List every assumption and ambiguity.
**GATE: present the plan. No code. Wait for approval.**

### Phase 1 — Project scaffold *(Sonnet 5)*
`.uproject`, all six modules with `.Build.cs`, both `.Target.cs`, `.gitignore`, `.gitattributes` with LFS rules for `uasset`/`umap`/`fbx`/`wav`/`png`/`tga`, config `.ini` files including the physics substepping block, Enhanced Input action/context assets defined in C++.
**GATE: confirm `Development Editor` compiles. Stop.**

### Phase 2 — Vehicle data architecture *(Sonnet 5)*
All config structs, `UVehicleSetupDataAsset`, `UVehicleFeelDataAsset`, `UVehicleSetupApplier`, a `UDataValidator` enforcing physical sanity (positive mass, monotonic gear ratios, curve domain coverage), and `Tools/editor_python/validate_vehicle_data.py`.
**GATE: print the full Data Asset schema and validation rules. Stop.**

### Phase 3 — Vehicle core *(Sonnet 5)*
`AMidanVehiclePawn`, movement-component configuration from data, `UVehicleAeroComponent` with downforce and drag in the async physics callback, surface detection via physical materials, input handling with steering-curve shaping, assists (TC/ABS/stability) as toggleable data-driven systems.
**GATE: list every physics-callback operation and confirm zero allocations. Provide the manual steps to rig and import a vehicle. Stop.**

### Phase 4 — Feel layer *(Opus 5)*
`UMidanChaseCameraComponent` with all seven behaviours, four camera modes, `UVehicleAudioComponent` with the two-axis parameter mapping, FX component (particles, decals with pooling, backfire), force feedback.
**GATE: print every curve the human must author, with recommended shapes and ranges. Stop.**

### Phase 5 — Track & race systems *(Sonnet 5)*
`AMidanTrackSpline` with spline-mesh road generation, checkpoint generation tool, `UMidanLapTimingSubsystem` with sequence validation and sector splits, off-track and respawn, race GameMode/GameState/PlayerState with Gameplay Tag states, grid spline, 10Hz position calculation.
**GATE: run the Automation Specs for lap validation. Print results. Stop.**

### Phase 6 — AI opponents *(Sonnet 5)*
Racing-line spline data structure, the curvature-based speed-profile generator with backward braking pass, pure-pursuit lateral controller, PID longitudinal controller with anti-windup, overtaking lane logic, predictive avoidance, difficulty configuration, mistake model, telemetry logging of rubber-banding.
**GATE: print the speed-profile output for a synthetic corner sequence and the PID convergence test results. Stop.**

### Phase 7 — Race flow & UI *(Sonnet 5)*
HUD (speed, gear, RPM, lap, position, sector delta, mini-map from spline), countdown, results screen, pause, settings menu backed by `UGameUserSettings`, `UMidanSaveGame` for best laps and ghosts.
**GATE: screenshot descriptions + the settings schema. Stop.**

### Phase 8 — Rendering & performance *(Opus 5)*
Scalability tiers, Lumen/VSM/TSR configuration per tier, streaming and HLOD commandlet automation, master-material policy, profiling harness, deterministic hot-lap replay, `Tools/analysis/perf_report.py`.
**GATE: run the profiling harness. Print the measured budget table with pass/fail per line. Do not proceed if total frame time exceeds budget — fix it first.**

### Phase 9 — Telemetry & analysis *(Sonnet 5)*
Telemetry subsystem with async flush, binary + CSV serialisation, ghost record/replay from inputs with resync, `telemetry_report.py` producing all six chart types.
**GATE: capture one lap, print the report, verify zero game-thread I/O. Stop.**

### Phase 10 — Build, CI & ship *(Sonnet 5 → Haiku 4.5 for docs)*
`build.py`, `package_shipping.py`, `generate_pso_cache.py`, `verify_build.py`, Automation Specs and Functional Tests wired to `RunUAT RunUnreal`, GitHub Actions self-hosted workflow, `BUILD_RUNBOOK.md`, itch.io Butler upload script, README with measured performance table and asset licences.
**GATE: produce a Shipping build, run verification, report build size, launch time, hitch count before and after PSO caching. Final file summary.**

---

## Constraints

- Only do what the current phase requires. No speculative abstraction.
- No TODOs, no stubbed functions presented as complete. If something needs the editor, say so in `MANUAL_STEPS.md` — do not fake it.
- Every class gets a header comment stating its responsibility and its single reason to change.
- `UPROPERTY`/`UFUNCTION` macros correct and complete; no raw pointers to `UObject`s — use `TObjectPtr`.
- Forward-declare in headers; include in `.cpp`. Keep compile times sane.
- Const-correct. `virtual`/`override` explicit.
- Every system that ticks must justify it at the gate.

## Acceptance Criteria

- [ ] All six modules compile clean in `Development Editor` and `Shipping`
- [ ] Three vehicles fully defined by Data Assets, zero tuning values in C++
- [ ] Physics substepping and async tick enabled; zero allocations in physics callbacks
- [ ] AI drives through the identical input path as the player — no physics cheating
- [ ] Lap validation rejects corner-cutting and reverse-direction laps (Automation Spec proves it)
- [ ] Measured frame budget table meets or beats §5.1 targets
- [ ] Telemetry captures at 60Hz with zero game-thread I/O
- [ ] Ghost replays from inputs, not transforms
- [ ] Shipping build produced by one command, verified by script
- [ ] PSO cache bundled; hitch count measured before and after
- [ ] No real manufacturer, model, or logo anywhere
- [ ] Every asset licence recorded in `docs/ASSET_LICENCES.md`
- [ ] `MANUAL_STEPS.md` is complete enough for someone else to reproduce the build

## Stop Conditions

Pause and ask before: deleting a file · adding a plugin or third-party dependency · changing the module dependency graph · changing the frame budget targets · anything requiring the editor that you might be tempted to fake · any architecture fork with two valid paths · any error unresolved after 2 attempts · moving to the next phase.

Think carefully before starting Phase 0.

---

---

# PART 2 — AFTER THE BUILD (do not paste)

## §3.5 `docs/VEHICLE_SPEC.md` template (write this by hand before Phase 0)

```markdown
# Vehicle Roster — 3 cars, deliberately different personalities

## 1. [Fictional name] — Hypercar
Class: Hyper | Drivetrain: AWD | Mass: ~1400kg | Power: ~800hp
Personality: brutal straight-line speed, high downforce, planted at speed,
nervous below 80km/h. Rewards commitment. Punishes lift-off mid-corner.
Torque curve: peaky, strong top end.
Handling bias: mild understeer under power, neutral off-throttle.

## 2. [Fictional name] — GT
Class: GT | Drivetrain: RWD | Mass: ~1550kg | Power: ~500hp
Personality: the drift car. Progressive oversteer, controllable slides,
forgiving at the limit. The one a player will spend the most time in.
Torque curve: flat, wide.
Handling bias: rear friction below front. Oversteer character.

## 3. [Fictional name] — Rally
Class: Rally | Drivetrain: AWD | Mass: ~1250kg | Power: ~350hp
Personality: slow on tarmac, unstoppable on gravel. High suspension travel,
visible body movement, high COM — deliberately.
Torque curve: torquey low-end.
Handling bias: surface-dependent — the car that makes surface materials matter.
```

Three cars that feel *the same* is a worse portfolio than one car that feels great. Make them argue with each other.

## The 12-week reality check

Phases 0–4 are roughly weeks 1–6, and week 5–7 is **handling tuning**, which Claude Code cannot do for you. Budget three full weeks of your own hands on the wheel. Playground and Codemasters employ dedicated vehicle handling specialists; treating this as an afternoon of number-twiddling is the single most common way an indie racing game ends up feeling wrong.

## What actually gets you hired from this

Not the track. Not the graphics. These three things, in order:

1. **The car feels good.** A recruiter drives for 90 seconds. That's the whole evaluation.
2. **The measured performance table in your README.** Almost no portfolio has one. It says "I ship."
3. **The telemetry + analysis layer.** This is your differentiator — an AI/ML engineer's fingerprint on a game project. Nobody else's racing portfolio has a player-vs-AI delta trace.

## Interview positioning

> "Midan is a UE5 racing vertical slice where every tuning value lives in Data Assets rather than C++, so handling is diffable in version control and sweepable programmatically. The AI drives through the identical input path as the player — no physics cheating — using pure-pursuit lateral control and a PID longitudinal controller against a speed profile generated from curvature with a backward braking pass. I hold a documented 16.6ms frame budget with a deterministic hot-lap replay so performance runs are comparable across builds, and I ship a bundled PSO cache — hitch count dropped from X to Y. The telemetry layer captures at 60Hz off the game thread and produces player-vs-AI delta traces, which is how I tuned the AI difficulty curve."

That paragraph is what a technical director wants to hear. Everything in this prompt exists to make it true.

## The four ways this build fails

1. **Skipping physics substepping.** You will spend two weeks fighting instability you configured in on day one.
2. **Tuning values creeping into C++.** The moment it happens, the data pipeline is dead and so is your sweepability story.
3. **Building the track before the car feels right.** A beautiful track around a bad-feeling car is a worse portfolio than a grey-box loop around a great one.
4. **Skipping the PSO cache.** Your build will hitch, reviewers will assume incompetence, and the fix is a scripted afternoon.
