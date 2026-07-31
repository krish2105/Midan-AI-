# Midan — Architecture

Module dependency graph and full class inventory. **Phase 0 deliverable. No code exists
yet** — this document is the contract Phases 1–10 implement against.

Engine: **UE 5.8**. Conventions: `CLAUDE.md`. Frame budget: `docs/PERFORMANCE_BUDGET.md`.

---

## 1. The organising principle

Systems and algorithms in C++. Every tuning value in a Data Asset. Blueprints are thin
subclasses that carry asset references and nothing else.

That split is what makes handling diffable in version control, validatable in CI, and
**sweepable programmatically** — which is the precondition for the telemetry work in
Phase 9. It is also the rule most likely to erode under deadline pressure, so it is
enforced structurally: every tuning asset derives from `UMidanDataAsset` and must
implement `ValidateData()`, and `MidanEditorTools` routes those validators into the
editor's Data Validation framework so CI can fail on a bad asset.

---

## 2. Modules

### 2.1 Dependency graph

The master prompt fixes `MidanCore → nothing`, `MidanVehicle → Core`,
`MidanAI → Core + Vehicle`, and `MidanEditorTools` as editor-only. It is **silent on
`MidanRace` and `MidanTelemetry`**. Rather than widen the graph — which is a stop
condition — both are resolved with interfaces declared in `MidanCore` and a world-scoped
service locator.

```
                          ┌──────────────┐
                          │  MidanCore   │   engine only: Core, CoreUObject,
                          │              │   Engine, GameplayTags,
                          │              │   DeveloperSettings, PhysicsCore
                          └───┬───┬───┬──┘
              ┌───────────────┘   │   └───────────────┐
              ▼                   ▼                   ▼
     ┌────────────────┐   ┌──────────────┐   ┌──────────────────┐
     │  MidanVehicle  │   │  MidanRace   │   │  MidanTelemetry  │
     └───────┬────────┘   └──────────────┘   └──────────────────┘
             │
             ▼
     ┌────────────────┐
     │    MidanAI     │   Core + Vehicle
     └────────────────┘

     ┌──────────────────────────────────────────────────────────┐
     │  MidanEditorTools           EDITOR TARGET ONLY           │
     │  → Core, Vehicle, Race, AI, UnrealEd, Slate, SlateCore,  │
     │    EditorScriptingUtilities, AssetRegistry, DataValidation│
     └──────────────────────────────────────────────────────────┘
```

**No module hard-links a sibling.** All cross-module reads resolve through
`UMidanServiceLocatorSubsystem` (a `UWorldSubsystem` in Core) holding interface pointers:

| Consumer | Needs | Reads through |
|---|---|---|
| `MidanRace` | vehicle speed, wheel surface, frame state | `IMidanVehicleInterface` |
| `MidanTelemetry` | per-vehicle frame state at 60 Hz | `IMidanTelemetrySource` |
| `MidanAI` | race phase (don't drive during countdown), lap/position | `IMidanRaceStateInterface` |
| `MidanAI` | rubber-band event logging | `IMidanTelemetrySink` |
| `MidanAI`, `MidanRace` | track arc length, curvature, width | `IMidanTrackInterface` |

**Cost of this choice:** one indirection on cross-module reads, and interface registration
at `BeginPlay`. **Benefit:** every module compiles and unit-tests standalone, and the
dependency graph stays exactly as specified. The simpler alternative — `MidanRace`
linking `MidanVehicle` directly — is recorded as assumption A7 in `docs/ASSUMPTIONS.md`.

**`MidanEditorTools` is absent from the runtime target.** Its absence from a Shipping
binary is a hard gate in `Tools/build/verify_build.py` (Phase 10).

**`MidanTests`** (Phase 10) → `Core`, `Vehicle`, `Race`, `AI`, engine `FunctionalTesting`.
Map-based functional tests (`docs/ARCHITECTURE.md` §5's lower half — vehicle spawn, AI lap
completion, respawn, race completion). Added to `Midan.Target.cs` only when
`Target.Configuration != Shipping`, and unconditionally to `MidanEditor.Target.cs` (an
Editor target is never Shipping). Same absence-from-Shipping gate as `MidanEditorTools`,
extended by `verify_build.py` to cover both.

### 2.2 Targets

| File | Type | Extra modules |
|---|---|---|
| `Source/Midan.Target.cs` | `TargetType.Game` | MidanCore, MidanVehicle, MidanRace, MidanAI, MidanTelemetry **+ MidanTests when not Shipping** |
| `Source/MidanEditor.Target.cs` | `TargetType.Editor` | the five above **+ MidanEditorTools + MidanTests** |

### 2.3 Tick inventory — pre-committed

Every ticking system must justify itself at its phase gate. This is the approved list;
anything not on it is event- or timer-driven, and additions need justification.

| System | Cadence | Justification |
|---|---|---|
| `UVehicleAeroComponent` | async physics callback @ 120 Hz | Force application must be substep-coherent |
| `UVehicleSurfaceSensorComponent` | async physics callback | Reads wheel contact from the same substep |
| `UVehicleAssistComponent` | async physics callback | TC/ABS act on per-substep slip, not per-frame slip |
| `UVehicleInputComponent` | Enhanced Input events + per-frame shaping | Rate-limited interpolation needs frame dt |
| `UMidanChaseCameraComponent` | per-frame | Camera is a render-rate concern by definition |
| `UVehicleAudioComponent` | per-frame | Parameter push into the MetaSound graph |
| `UVehicleFXComponent` | per-frame, budget-gated | Particle rates and decal deposition are visual-rate |
| `AMidanRaceGameState` position calc | **timer @ 10 Hz** | Explicitly not per-frame, per master prompt §3.3 |
| `UMidanLapTimingSubsystem` | checkpoint overlap events + off-track grace timer | Event-driven |
| `AMidanOpponentController` | per-frame, **staggered across the 7 opponents** | Control loop; the stagger keeps game-thread cost flat |
| `UMidanTelemetrySubsystem` | fixed **60 Hz** accumulator | Decoupled from frame rate by spec |
| `UMidanHUDWidget` and every state-driven UI widget (`UMidanCountdownWidget`, `UMidanResultsWidget`) | UMG `NativeTick`, per-frame | Reading live render-rate state (speed, RPM, position) is what a HUD is; UMG widget Tick is the standard mechanism for it, the same exemption already granted to `UMidanChaseCameraComponent`. Added at Phase 7. |

---

## 3. Class inventory

### 3.1 `MidanCore`

Shared types, interfaces, tags, math. Depends on nothing project-side. **Contains no
gameplay behaviour** — if a class here starts making decisions, it belongs in another
module.

| File | Type | Responsibility / single reason to change |
|---|---|---|
| `MidanCoreModule.h/.cpp` | `IModuleInterface` | Module lifetime. Changes when startup ordering changes. |
| `MidanGameplayTags.h/.cpp` | native tag declarations | The tag vocabulary. Changes when a new state or class exists. |
| `MidanLogChannels.h/.cpp` | log categories | One channel per module, declared centrally. |
| `MidanCoreTypes.h` | POD structs | The data contract between modules. Changes when a system needs a field it cannot derive. |
| `MidanVehicleInterface.h` | `IMidanVehicleInterface` | What a racer must expose. |
| `MidanTrackInterface.h` | `IMidanTrackInterface` | What a track must expose. |
| `MidanRaceStateInterface.h` | `IMidanRaceStateInterface` | What race phase must expose. |
| `MidanTelemetryInterfaces.h` | `IMidanTelemetrySource`, `IMidanTelemetrySink` | Capture pull + event push. |
| `MidanServiceLocator.h/.cpp` | `UMidanServiceLocatorSubsystem : UWorldSubsystem` | Register/resolve the interfaces above. Changes when a new interface crosses a module boundary. |
| `MidanDataAsset.h/.cpp` | `UMidanDataAsset : UPrimaryDataAsset` | Root of every tuning asset. `virtual bool ValidateData(TArray<FText>& OutErrors) const`. |
| `MidanCurveUtils.h/.cpp` | free functions | `EvalNormalised`, `ValidateCurveDomain`. Allocation-free; hot-path functions header-inlined. |
| `MidanMathUtils.h` | free functions | `SafeDivide`, `MapRangeClamped`, `ExpDamp` (frame-rate-independent damping). Header-only. |
| `MidanDeveloperSettings.h/.cpp` | `UMidanDeveloperSettings : UDeveloperSettings` | Project config that is **not** tuning: telemetry rate, debug flags, `TSoftObjectPtr` defaults. |

**Native gameplay tags** (`MidanGameplayTags.h`):

```
Vehicle.Class.Hyper / GT / Rally
Vehicle.Camera.ChaseFar / ChaseNear / Bonnet / Cockpit
Vehicle.Assist.TractionControl / ABS / Stability / SteeringAssist
Race.State.Grid / Countdown / Racing / Finished / Results
Surface.Tarmac / Kerb / Gravel / Grass / Sand / Wet
Telemetry.Event.RubberBand / Mistake / OffTrack / LapInvalidated / Respawn
```

**Shared PODs** (`MidanCoreTypes.h`):

| Struct | Fields |
|---|---|
| `FMidanVehicleInputState` | Throttle, Brake, Steer, Handbrake (all normalised), bShiftUp, bShiftDown |
| `FMidanWheelState` | SlipRatio, SlipAngle, Load, SurfaceTag, bInContact |
| `FMidanVehicleFrameState` | Transform, LinearVelocity, AngularVelocity, EngineRPM, Gear, LateralG, LongitudinalG, `FMidanWheelState[4]` |
| `FMidanTrackPosition` | ArcLength, LapIndex, SectorIndex, LateralOffset |

`FMidanVehicleInputState` is the **single input contract**. The player's
`UVehicleInputComponent` and the AI's `AMidanOpponentController` both produce one and
pass it to `IMidanVehicleInterface::ApplyInput`. There is no second path — this is the
structural guarantee that the AI cannot cheat physics.

### 3.2 `MidanVehicle`

Vehicle pawn, physics configuration, feel systems.
Deps at Phase 1: `MidanCore`, `ChaosVehicles`, `PhysicsCore`, `GameplayTags`, `InputCore`.
Feature deps are added at the phase that needs them — `EnhancedInput` at Phase 3,
`Niagara` and `AudioMixer` at Phase 4 — rather than declared speculatively.

**Config structs** — `Public/Config/`, one header each, all `USTRUCT(BlueprintType)`.
Every field carries a doc comment stating its **handling consequence**, not its units.

| Struct | Contains |
|---|---|
| `FVehicleMassConfig` | Mass, CentreOfMassOffset, InertiaTensorScale |
| `FVehiclePowertrainConfig` | TorqueCurve, MaxRPM, EngineIdleRPM, EngineBrakeEffect, GearRatios[], FinalRatio, ChangeUpTime, ChangeDownTime |
| `FVehicleDrivetrainConfig` | DifferentialType, FrontRearSplit, FrontLeftRightSplit, RearLeftRightSplit |
| `FVehicleSuspensionConfig` | `FVehicleAxleSuspensionConfig` Front, Rear |
| `FVehicleAxleSuspensionConfig` | MaxRaise, MaxDrop, SpringRate, SpringPreload, DampingRatio, ForceOffset, WheelLoadRatio |
| `FVehicleTyreConfig` | `FVehicleAxleTyreConfig` Front, Rear + derived `FrontRearFrictionRatio` (the understeer/oversteer dial, exposed as one sweepable value) |
| `FVehicleAxleTyreConfig` | FrictionForceMultiplier, LateralSlipGraph, CorneringStiffness, SlipThreshold, SkidThreshold |
| `FVehicleSteeringConfig` | SteeringCurve (max angle vs speed — **mandatory**), KeyboardShapingCurve, SteeringInputRiseRate, SteeringInputFallRate, AckermannAccuracy |
| `FVehicleAeroConfig` | DragCoefficient, FrontalArea, AirDensity, FrontLiftCoefficient, RearLiftCoefficient, CentreOfPressureOffset, FrontApplicationOffset, RearApplicationOffset |
| `FVehicleAssistConfig` | Per-assist enable flag + gains for TC, ABS, stability, steering assist |

**Runtime classes:**

| File | Type | Responsibility / single reason to change |
|---|---|---|
| `VehicleSetupDataAsset.h/.cpp` | `UVehicleSetupDataAsset : UMidanDataAsset` | The source of truth for how a car drives. Exactly the master-prompt §1.1 schema. |
| `VehicleFeelDataAsset.h/.cpp` | `UVehicleFeelDataAsset : UMidanDataAsset` | Camera curves, audio params, FX params, force-feedback soft refs. |
| `SurfaceResponseDataAsset.h/.cpp` | `USurfaceResponseDataAsset`, `FSurfaceResponseRow` | Surface tag → friction multiplier, rolling resistance, SFX / Niagara / decal / FFB soft refs. |
| `MidanVehiclePawn.h/.cpp` | `AMidanVehiclePawn : AWheeledVehiclePawn` | The racer. Implements `IMidanVehicleInterface` + `IMidanTelemetrySource`. |
| `MidanVehicleMovementComponent.h/.cpp` | `: UChaosWheeledVehicleMovementComponent` | Thin subclass exposing the async-callback hook point. |
| `VehicleSetupApplier.h/.cpp` | `UVehicleSetupApplier` | Writes Data Asset → Chaos component + wheel setups. `#if WITH_EDITOR` hot-reload delegate. **The Data Asset is upstream; Chaos is a downstream consumer.** |
| `VehicleAeroComponent.h/.cpp` | `UActorComponent` | Drag + separate front/rear downforce in the async physics callback. Front/rear balance shifting with speed is what makes a fast car feel planted. |
| `VehicleSurfaceSensorComponent.h/.cpp` | `UActorComponent` | Per-wheel physical-material sampling → surface tag + off-track wheel fraction. **Physical materials, never trigger volumes.** |
| `VehicleInputComponent.h/.cpp` | `UActorComponent` | Enhanced Input → `FMidanVehicleInputState`. Steering curve, separate keyboard shaping curve, rise/fall rate limits. |
| `VehicleAssistComponent.h/.cpp` | `UActorComponent` | TC / ABS / stability / steering assist. Each individually toggleable, every gain from data. |
| `MidanChaseCameraComponent.h/.cpp` | `USceneComponent` | Speed FOV, positional lag, look-ahead, slip yaw, vertical damping, impact shake, surface rumble. |
| `VehicleCameraModeInterface.h` | `IMidanVehicleCameraMode` | Chase far / chase near / bonnet / cockpit behind one interface. |
| `VehicleAudioComponent.h/.cpp` | `UActorComponent` | Two-axis (RPM × engine load) MetaSound parameter push. Sound design stays in the graph; mapping stays in C++. |
| `VehicleFXComponent.h/.cpp` | `UActorComponent` | Tyre smoke, surface particles, **pooled** skid decals, exhaust backfire. |
| `VehicleHapticsComponent.h/.cpp` | `UActorComponent` | Force-feedback curve driver: idle rumble, surface roughness, slip, impacts, kerbs. |
| `MidanInputConfigDataAsset.h/.cpp` | `UMidanDataAsset` subclass | `UInputAction` soft refs + `UInputMappingContext`. |

### 3.3 `MidanRace`

Track, checkpoints, lap timing, race flow. Deps: `MidanCore`, `GameplayTags`.

| File | Type | Responsibility / single reason to change |
|---|---|---|
| `MidanTrackSpline.h/.cpp` | `AMidanTrackSpline` | Spline + spline-mesh road. Implements `IMidanTrackInterface`. Exposes width, banking, material-per-section. |
| `MidanTrackSectionData.h` | `FMidanTrackSection` | StartDistance, Width, Banking, MaterialIndex. |
| `MidanCheckpoint.h/.cpp` | `AMidanCheckpoint` | Index, sector index, arc length, half-width. Generated by the Phase 5 Python tool, named deterministically. |
| `MidanLapRecord.h` | `FMidanLapRecord`, `FMidanSectorTime` | Lap and sector time records. |
| `MidanLapTimingSubsystem.h/.cpp` | `UWorldSubsystem` | Checkpoint sequence validation (rejects corner-cutting and reverse direction), 3 sectors, best-sector, theoretical best, off-track grace, invalidation. |
| `MidanRespawnComponent.h/.cpp` | `UActorComponent` | Rewind to last valid checkpoint with velocity and rotation restored, on a cooldown. |
| `MidanPositionCalculator.h/.cpp` | free functions | Arc-length + lap count → race position. **No `UWorld` dependency — unit-testable without a map.** |
| `MidanRaceGameMode.h/.cpp` | `AMidanRaceGameMode` | Grid population and phase transitions. |
| `MidanRaceGameState.h/.cpp` | `AMidanRaceGameState` | Implements `IMidanRaceStateInterface`. Gameplay-tag state machine. 10 Hz position timer. |
| `MidanRacePlayerState.h/.cpp` | `AMidanRacePlayerState` | Per-racer lap, sector, position, best times. |
| `MidanGridSpline.h/.cpp` | `AMidanGridSpline` | Data-driven grid slots. 8 slots: player + 7 AI. |
| `MidanRaceRulesDataAsset.h/.cpp` | `UMidanDataAsset` subclass | LapCount, CountdownDuration, OffTrackGraceSeconds, RespawnCooldown, SectorCount, PositionUpdateHz. |

Race state machine: `Race.State.Grid` → `Countdown` → `Racing` → `Finished` → `Results`.

### 3.4 `MidanAI`

Racing line and opponent control. Deps: `MidanCore`, `MidanVehicle`, `GameplayTags`,
`AIModule` (supplies `AAIController`, which `AMidanOpponentController` derives from — an
engine module, so it does not affect the project-module graph).

**No Behaviour Trees.** Racing is a continuous control problem, not a discrete decision
problem.

| File | Type | Responsibility / single reason to change |
|---|---|---|
| `MidanRacingLineData.h` | `FRacingLinePoint`, `FRacingLineSample` | TargetSpeed, bBrakingZone, LateralOffsetMin/Max per point. |
| `MidanRacingLineSpline.h/.cpp` | `AMidanRacingLineSpline` | A separate spline from the road centre, carrying per-point racing-line data. |
| `MidanSpeedProfileGenerator.h/.cpp` | free functions | Curvature lookahead + **backward braking pass**. `v_target(s) = min(v_max, sqrt(µ·g/|κ(s)|), backward_pass_limit(s))`. **Pure math, unit-testable.** |
| `MidanRacingLineFollower.h/.cpp` | `UMidanRacingLineFollower` | Pure-pursuit lateral control with speed-dependent lookahead. |
| `MidanPIDController.h/.cpp` | `FMidanPIDController` | Longitudinal PID: separate throttle and brake gains, anti-windup clamp. **Plain struct, unit-testable.** |
| `MidanOpponentController.h/.cpp` | `AMidanOpponentController : AAIController` | Composes follower + PID. Outputs **only** a normalised `FMidanVehicleInputState` through `IMidanVehicleInterface::ApplyInput`. |
| `MidanOvertakeComponent.h/.cpp` | `UActorComponent` | Lateral offset lanes. Evaluates a lane change on closing-speed and gap thresholds with a predictive clear sweep. |
| `MidanAvoidanceComponent.h/.cpp` | `UActorComponent` | Predictive sphere traces 1.5 s along the projected path. Blends a lateral offset rather than braking where possible. |
| `MidanMistakeModel.h/.cpp` | `FMidanMistakeModel` | Probabilistic late brake / wide line, scaled inversely with difficulty. Perfect AI is boring AI. |
| `MidanRubberBandComponent.h/.cpp` | `UActorComponent` | Bounded speed-multiplier envelope decaying to 1.0. **Logs every application** to `IMidanTelemetrySink` so the subtlety is provable. |
| `MidanAIDifficultyDataAsset.h/.cpp` | `UMidanDataAsset` subclass | TyreFrictionMultiplier, ReactionDelay, TargetSpeedMultiplier, MistakeProbability, Aggression, RubberBandEnvelope, RubberBandDecayRate. |

**Hard architectural rule:** difficulty modifies friction, reaction delay, target speed,
mistake probability, and aggression. It never teleports, never adds engine power beyond
the player's car class, and rubber-banding stays inside a small envelope that decays to
1.0 within a few seconds.

### 3.5 `MidanTelemetry`

Capture, serialisation, ghost replay. Deps: `MidanCore`.

| File | Type | Responsibility / single reason to change |
|---|---|---|
| `MidanTelemetryFrame.h` | `FMidanTelemetryFrame` | Packed capture record: timestamp, transform, velocity, per-wheel slip ratio / slip angle / load / surface, throttle, brake, steer, handbrake, gear, RPM, lateral and longitudinal G, distance along racing line, lap, sector. |
| `MidanTelemetryRingBuffer.h/.cpp` | `FMidanTelemetryRingBuffer` | Preallocated single-producer/single-consumer ring. **Zero allocation in the write path.** |
| `MidanTelemetrySubsystem.h/.cpp` | `UGameInstanceSubsystem` | Implements `IMidanTelemetrySink`. Fixed **60 Hz** accumulator decoupled from frame rate. Enumerates `IMidanTelemetrySource` providers. |
| `MidanTelemetryWriter.h/.cpp` | `FMidanTelemetryBinaryWriter`, `FMidanTelemetryCsvExporter` | Versioned compact binary + a CSV export path for the Python tooling. |
| `MidanTelemetryFlushTask.h/.cpp` | `FNonAbandonableTask` | Async disk flush. **Never game-thread file I/O.** |
| `MidanGhostData.h` | `FMidanGhostRecording`, `FMidanGhostResyncKey` | Recorded inputs + periodic state resync keys. |
| `MidanGhostRecorder.h/.cpp` | `UActorComponent` | Records `FMidanVehicleInputState` per physics step + periodic `FMidanVehicleFrameState` keyframes. |
| `MidanGhostPlayer.h/.cpp` | `UActorComponent` | Replays **inputs** through `ApplyInput` with resync correction. Input replay demonstrates deterministic physics; transform replay is a cheap trick. |

### 3.6 `MidanEditorTools`

`Editor` module type, `WITH_EDITOR` only, **never referenced at runtime**.
Deps, all **private**: `MidanCore`, `MidanVehicle`, `MidanRace`, `MidanAI`,
`MidanTelemetry`, `UnrealEd`, `Slate`, `SlateCore`, `Projects`, `AssetRegistry`,
`EditorScriptingUtilities`, `DataValidation`, `GameplayTags`.

`MidanTelemetry` is included because Phase 8's `capture_perf_baseline.py` drives the
deterministic hot-lap replay through `MidanPerfCaptureLibrary`. Private-only dependencies
mean nothing outside this module can include its headers.

| File | Type | Responsibility / single reason to change |
|---|---|---|
| `MidanEditorToolsModule.h/.cpp` | `IModuleInterface` | Registers validators on startup. |
| `MidanDataValidators.h/.cpp` | `UEditorValidatorBase` subclasses | Route `UMidanDataAsset::ValidateData` into the editor Data Validation framework so CI can run `-run=DataValidation`. |
| `MidanCheckpointGeneratorLibrary.h/.cpp` | `UBlueprintFunctionLibrary` | Python-callable `GenerateCheckpoints(TrackSpline, Count, SectorCount)`. |
| `MidanRacingLineToolLibrary.h/.cpp` | `UBlueprintFunctionLibrary` | Python-callable wrapper over `MidanSpeedProfileGenerator`. |
| `MidanPerfCaptureLibrary.h/.cpp` | `UBlueprintFunctionLibrary` | Python-callable stat and Insights-trace capture triggers. |
| `MidanBuildHLODCommandlet.h/.cpp` | `UCommandlet` | Scripted HLOD level 0 and 1 build. |

The editor tooling is deliberately thin: each library is a Python-callable entry point
over math that lives in a runtime module. The algorithm is testable by an Automation Spec;
the editor wrapper is not, so it must contain no logic worth testing.

---

## 4. Physics configuration

From master prompt §1.3, into `Config/DefaultEngine.ini` at Phase 1:

```ini
[/Script/Engine.PhysicsSettings]
bSubstepping=True
MaxSubstepDeltaTime=0.008333        ; 120 Hz physics
MaxSubsteps=6
bTickPhysicsAsync=True
AsyncFixedTimeStepSize=0.008333
```

Rules that follow from it:

- All vehicle force application happens in the **async physics callback**, never in `Tick`.
- Chassis collision is a **convex hull**, not complex. Wheels are sphere or capsule.
- The physics asset must weight bones correctly — bad bone weighting is the primary cause
  of a vehicle that flips unpredictably.
- Never scale a vehicle actor at runtime.

`bTickPhysicsAsync` combined with Chaos Vehicles has had version-specific caveats; see
assumption A18 in `docs/ASSUMPTIONS.md`.

---

## 5. Test surface

Designed in now, because a class that needs a `UWorld` to test usually did not need one.

| Test | Type | Target |
|---|---|---|
| Lap validation, checkpoint sequencing | Automation Spec | `MidanPositionCalculator`, `UMidanLapTimingSubsystem` |
| Racing-line speed-profile math | Automation Spec | `MidanSpeedProfileGenerator` |
| PID convergence and anti-windup | Automation Spec | `FMidanPIDController` |
| Telemetry serialisation round-trip | Automation Spec | `FMidanTelemetryBinaryWriter` |
| Data Asset validation rules | Automation Spec + `-run=DataValidation` | `UMidanDataAsset` subclasses |
| Vehicle spawns and drives | Functional Test | `AVehicleSpawnFunctionalTest` (`MidanTests`, Phase 10) |
| AI completes a lap without leaving track | Functional Test | `AAILapCompletionFunctionalTest` (`MidanTests`, Phase 10) |
| Respawn restores valid state | Functional Test | `ARespawnFunctionalTest` (`MidanTests`, Phase 10) |
| Race completes and produces results | Functional Test | `ARaceCompletionFunctionalTest` (`MidanTests`, Phase 10) |
| Deterministic hot-lap performance run | Gauntlet | `MidanHotLapReplay` |

The four Automation Spec targets in the top half are all pure math or pure serialisation.
That is not an accident — it is why `MidanPositionCalculator`, `MidanSpeedProfileGenerator`,
and `FMidanPIDController` are free functions and plain structs rather than components.
