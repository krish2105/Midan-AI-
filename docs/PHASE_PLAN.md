# Midan — File-Level Plan, Phases 1–10

Every phase ends at a **gate**. A gate outputs: a `✅` summary, the files created,
**measured** numbers where applicable, and a new section in `docs/MANUAL_STEPS.md`.
Nothing proceeds past a gate without approval.

Class responsibilities are in `docs/ARCHITECTURE.md` §3 and are not repeated here — this
document is the file manifest and the order of work.

| Phase | Status | Model | Deliverable |
|---|---|---|---|
| 0 | **complete** | Opus 5 | Architecture, budget, conventions, this plan |
| 1 | **files written · gate BLOCKED on engine install** | Opus 5 | Project scaffold, six modules compile |
| 2 | **files written · gate BLOCKED on engine install** | Sonnet 5 | Vehicle data architecture |
| 3 | **files written · gate BLOCKED on engine install** | Sonnet 5 | Vehicle core |
| 4 | **files written · gate BLOCKED on engine install** | Opus 5 | Feel layer |
| 5 | not started | Sonnet 5 | Track & race systems |
| 6 | not started | Sonnet 5 | AI opponents |
| 7 | not started | Sonnet 5 | Race flow & UI |
| 8 | not started | Opus 5 | Rendering & performance |
| 9 | not started | Sonnet 5 | Telemetry & analysis |
| 10 | not started | Sonnet 5 → Haiku 4.5 | Build, CI & ship |

---

## Phase 1 — Project scaffold

**Creates:**

| Path | Notes |
|---|---|
| `Midan.uproject` | `EngineAssociation: 5.8`. Plugins: ChaosVehiclesPlugin, EnhancedInput, GameplayTags, PCG, MetaSound, PythonScriptPlugin, EditorScriptingUtilities |
| `Source/Midan.Target.cs` | `TargetType.Game`. Five runtime modules. **No MidanEditorTools** |
| `Source/MidanEditor.Target.cs` | `TargetType.Editor`. Adds MidanEditorTools |
| `Source/MidanCore/MidanCore.Build.cs` + `Public/` + `Private/` + module `.h/.cpp` | |
| `Source/MidanVehicle/…` | same shape |
| `Source/MidanRace/…` | same shape |
| `Source/MidanAI/…` | same shape |
| `Source/MidanTelemetry/…` | same shape |
| `Source/MidanEditorTools/…` | `ModuleType.Editor`, `LoadingPhase.PostEngineInit` |
| `Config/DefaultEngine.ini` | Includes the §1.3 physics substepping block verbatim |
| `Config/DefaultGame.ini` | |
| `Config/DefaultInput.ini` | Enhanced Input default context |
| `Config/DefaultScalability.ini` | Four tier stubs; populated at Phase 8 |
| `Content/.gitkeep` | LFS-tracked directory placeholder |
| `README.md` | Skeleton; measured table added at Phase 10 |
| `docs/ASSET_LICENCES.md` | Empty table with a header. Populated on the same commit as each asset |

**Modifies:**

- `.gitattributes` — add `*.uexp`, `*.ubulk`, `*.blend`, `*.psd`, `*.jpg`, `*.mp3`,
  `*.ogg`, `*.hdr` alongside the existing seven LFS rules.
- `.gitignore` — already correct (`Binaries/ Intermediate/ Saved/ DerivedDataCache/
  Build/ *.xcworkspace *.xcodeproj DerivedData/ .DS_Store`). **Leave alone.**

**Deletes:** nothing. `Source/` and `Config/` do not currently exist.

### Blank C++ template deletion map

Only applies if the UE Blank C++ template is generated into this repo before Phase 1 runs.
The template produces a single module named after the project.

| Template file | Action |
|---|---|
| `Source/Midan/Midan.Build.cs` | **Delete** — superseded by the six per-module Build.cs files |
| `Source/Midan/Midan.h` and `Midan.cpp` | **Delete** — module implementation moves to `MidanCore` |
| `Source/Midan/MidanGameModeBase.h` and `.cpp` | **Delete** — replaced by `AMidanRaceGameMode` in `MidanRace` (Phase 5) |
| `Source/Midan/` directory | **Delete** once the three above are gone |
| `Source/Midan.Target.cs` | **Modify** — replace `ExtraModuleNames.Add("Midan")` with the five runtime modules |
| `Source/MidanEditor.Target.cs` | **Modify** — same, plus `MidanEditorTools` |
| `Config/Default*.ini` | **Modify in place.** Never delete a template `.ini`; append the locked blocks. The template writes engine-version-specific keys worth keeping |

Deleting a file is a stop condition — confirm before executing this map.

**Gate:** confirm `Development Editor` compiles clean, all six modules. Report the compile
command and its output. Stop.

---

## Phase 2 — Vehicle data architecture

**Creates:**

| Path |
|---|
| `Source/MidanCore/Public/MidanDataAsset.h` + `Private/MidanDataAsset.cpp` |
| `Source/MidanCore/Public/MidanCurveUtils.h` + `Private/MidanCurveUtils.cpp` |
| `Source/MidanCore/Public/MidanMathUtils.h` |
| `Source/MidanVehicle/Public/Config/VehicleMassConfig.h` |
| `Source/MidanVehicle/Public/Config/VehiclePowertrainConfig.h` |
| `Source/MidanVehicle/Public/Config/VehicleDrivetrainConfig.h` |
| `Source/MidanVehicle/Public/Config/VehicleSuspensionConfig.h` |
| `Source/MidanVehicle/Public/Config/VehicleTyreConfig.h` |
| `Source/MidanVehicle/Public/Config/VehicleSteeringConfig.h` |
| `Source/MidanVehicle/Public/Config/VehicleAeroConfig.h` |
| `Source/MidanVehicle/Public/Config/VehicleAssistConfig.h` |
| `Source/MidanVehicle/Public/VehicleSetupDataAsset.h` + `.cpp` |
| `Source/MidanVehicle/Public/VehicleFeelDataAsset.h` + `.cpp` |
| `Source/MidanVehicle/Public/SurfaceResponseDataAsset.h` + `.cpp` |
| `Source/MidanVehicle/Public/VehicleSetupApplier.h` + `.cpp` |
| `Source/MidanEditorTools/Public/MidanDataValidators.h` + `.cpp` |
| `Source/MidanVehicle/Private/Tests/VehicleDataValidationSpec.cpp` |
| `Tools/editor_python/validate_vehicle_data.py` |

**Validation rules enforced by `ValidateData()`:**

- `Mass > 0`
- Gear ratios monotonically decreasing, at least 2 gears, `FinalRatio > 0`
- `EngineIdleRPM < MaxRPM`, both positive
- Torque curve domain covers `[EngineIdleRPM, MaxRPM]`; no negative torque in range
- Steering curve domain covers `[0, expected top speed]`; monotonically non-increasing
- Front and rear `FrictionForceMultiplier` both `> 0`
- Suspension `MaxRaise + MaxDrop > 0`, `SpringRate > 0`, `DampingRatio` in `(0, 2]`
- `DisplayName` non-empty
- `VehicleClass` is a descendant of `Vehicle.Class`
- Every `TSoftObjectPtr` in the Feel asset either null or resolvable
- `CentreOfMassOffset` is **not** silently clamped — a high COM is a design dial, and the
  validator warns rather than errors

**Gate:** print the full Data Asset schema and every validation rule. Stop.

**Deviation from plan:** `MidanGameplayTags` and `MidanLogChannels` were pulled forward
from Phase 3 — the vehicle-class validation rule needs a native tag to compare against, and
the applier must log why it refused an invalid setup. See `docs/ASSUMPTIONS.md` A20.

---

## Phase 3 — Vehicle core

**Creates:**

| Path |
|---|
| ~~`MidanGameplayTags.h` + `.cpp`~~ — delivered in Phase 2 (A20) |
| ~~`MidanLogChannels.h`~~ — delivered in Phase 2 (A20) |
| `Source/MidanCore/Public/MidanCoreTypes.h` |
| `Source/MidanCore/Public/MidanVehicleInterface.h` |
| `Source/MidanCore/Public/MidanServiceLocator.h` + `.cpp` |
| `Source/MidanCore/Public/MidanDeveloperSettings.h` + `.cpp` |
| `Source/MidanVehicle/Public/MidanVehiclePawn.h` + `.cpp` |
| `Source/MidanVehicle/Public/MidanVehicleMovementComponent.h` + `.cpp` |
| `Source/MidanVehicle/Public/VehicleAeroComponent.h` + `.cpp` |
| `Source/MidanVehicle/Public/VehicleSurfaceSensorComponent.h` + `.cpp` |
| `Source/MidanVehicle/Public/VehicleInputComponent.h` + `.cpp` |
| `Source/MidanVehicle/Public/VehicleAssistComponent.h` + `.cpp` |
| `Source/MidanVehicle/Public/MidanInputConfigDataAsset.h` + `.cpp` |

Aero math, applied in the async physics callback at the configured application points:

- Drag: `-0.5 · ρ · Cd · A · v² · v̂` at the centre of pressure
- Downforce: `-0.5 · ρ · Cl · A · v²` at **separate front and rear** points

**Gate:** enumerate every operation performed inside the physics callback and confirm zero
heap allocations. Provide the Blender rig and FBX import manual steps. Stop.

---

## Phase 4 — Feel layer

**Creates:**

| Path |
|---|
| `Source/MidanVehicle/Public/VehicleCameraModeInterface.h` |
| `Source/MidanVehicle/Public/MidanChaseCameraComponent.h` + `.cpp` |
| `Source/MidanVehicle/Private/CameraModes/ChaseFarCameraMode.cpp` |
| `Source/MidanVehicle/Private/CameraModes/ChaseNearCameraMode.cpp` |
| `Source/MidanVehicle/Private/CameraModes/BonnetCameraMode.cpp` |
| `Source/MidanVehicle/Private/CameraModes/CockpitCameraMode.cpp` |
| `Source/MidanVehicle/Public/VehicleAudioComponent.h` + `.cpp` |
| `Source/MidanVehicle/Public/VehicleFXComponent.h` + `.cpp` |
| `Source/MidanVehicle/Public/VehicleHapticsComponent.h` + `.cpp` |

Camera defaults come from `docs/ART_DIRECTION.md` §5, which **overrides** master prompt
§2.1 where they conflict (base FOV 72° not 75° — assumption A11): FOV 72° → 95–100°,
spring arm 5.5–6.5 m, height 1.8–2.2 m, pitch −6° to −9°, location lag 8–12,
rotation lag 6–9, look-ahead yaw ±4°, slip yaw ±6°.

Audio is a **two-axis blend**: four RPM layers (idle, low, mid, high) crossfaded on RPM,
with **separate on-load and off-load sets** crossfaded by throttle. Two-axis blending is
what separates a convincing engine from a whine. Sound design stays in the MetaSound graph;
only the parameter mapping is C++.

Decals are **pooled with a fixed budget** — unbounded decal deposition is a slow leak that
only shows up on lap 4.

**Gate:** print every curve the human must author, with recommended shape and range for
each. Stop.

---

## Phase 5 — Track & race systems

**Creates:**

| Path |
|---|
| `Source/MidanCore/Public/MidanTrackInterface.h` |
| `Source/MidanCore/Public/MidanRaceStateInterface.h` |
| `Source/MidanRace/Public/MidanTrackSectionData.h` |
| `Source/MidanRace/Public/MidanTrackSpline.h` + `.cpp` |
| `Source/MidanRace/Public/MidanCheckpoint.h` + `.cpp` |
| `Source/MidanRace/Public/MidanLapRecord.h` |
| `Source/MidanRace/Public/MidanLapTimingSubsystem.h` + `.cpp` |
| `Source/MidanRace/Public/MidanRespawnComponent.h` + `.cpp` |
| `Source/MidanRace/Public/MidanPositionCalculator.h` + `.cpp` |
| `Source/MidanRace/Public/MidanRaceGameMode.h` + `.cpp` |
| `Source/MidanRace/Public/MidanRaceGameState.h` + `.cpp` |
| `Source/MidanRace/Public/MidanRacePlayerState.h` + `.cpp` |
| `Source/MidanRace/Public/MidanGridSpline.h` + `.cpp` |
| `Source/MidanRace/Public/MidanRaceRulesDataAsset.h` + `.cpp` |
| `Source/MidanRace/Private/Tests/LapValidationSpec.cpp` |
| `Source/MidanRace/Private/Tests/PositionCalculatorSpec.cpp` |
| `Source/MidanEditorTools/Public/MidanCheckpointGeneratorLibrary.h` + `.cpp` |
| `Tools/editor_python/batch_setup_checkpoints.py` |

Off-track detection reads the **physical material under each wheel** via
`UVehicleSurfaceSensorComponent`, with a grace-time threshold before a lap invalidates.
Not trigger volumes.

**Gate:** run the lap-validation Automation Specs. Print results, including the
corner-cutting and reverse-direction rejection cases. Stop.

---

## Phase 6 — AI opponents

**Creates:**

| Path |
|---|
| `Source/MidanAI/Public/MidanRacingLineData.h` |
| `Source/MidanAI/Public/MidanRacingLineSpline.h` + `.cpp` |
| `Source/MidanAI/Public/MidanSpeedProfileGenerator.h` + `.cpp` |
| `Source/MidanAI/Public/MidanRacingLineFollower.h` + `.cpp` |
| `Source/MidanAI/Public/MidanPIDController.h` + `.cpp` |
| `Source/MidanAI/Public/MidanOpponentController.h` + `.cpp` |
| `Source/MidanAI/Public/MidanOvertakeComponent.h` + `.cpp` |
| `Source/MidanAI/Public/MidanAvoidanceComponent.h` + `.cpp` |
| `Source/MidanAI/Public/MidanMistakeModel.h` + `.cpp` |
| `Source/MidanAI/Public/MidanRubberBandComponent.h` + `.cpp` |
| `Source/MidanAI/Public/MidanAIDifficultyDataAsset.h` + `.cpp` |
| `Source/MidanAI/Private/Tests/SpeedProfileSpec.cpp` |
| `Source/MidanAI/Private/Tests/PIDConvergenceSpec.cpp` |
| `Source/MidanEditorTools/Public/MidanRacingLineToolLibrary.h` + `.cpp` |
| `Tools/editor_python/generate_racing_line.py` |

Speed profile: `v_target(s) = min(v_max, sqrt(µ_effective · g / |κ(s)|), backward_pass_braking_limit(s))`,
with a backward pass from each apex computing braking points from deceleration capability.

The controller outputs **only** a normalised `FMidanVehicleInputState` fed through
`IMidanVehicleInterface::ApplyInput` — the identical path the player uses. Hard rule.

**Gate:** print the generated speed profile for a synthetic corner sequence and the PID
convergence test results. Stop.

---

## Phase 7 — Race flow & UI

**Creates:**

| Path |
|---|
| `Source/MidanRace/Public/UI/MidanHUD.h` + `.cpp` |
| `Source/MidanRace/Public/UI/MidanHUDDataAsset.h` + `.cpp` |
| `Source/MidanRace/Public/UI/MidanMinimapWidget.h` + `.cpp` |
| `Source/MidanRace/Public/UI/MidanCountdownWidget.h` + `.cpp` |
| `Source/MidanRace/Public/UI/MidanResultsWidget.h` + `.cpp` |
| `Source/MidanRace/Public/UI/MidanPauseWidget.h` + `.cpp` |
| `Source/MidanRace/Public/UI/MidanSettingsWidget.h` + `.cpp` |
| `Source/MidanCore/Public/MidanGameUserSettings.h` + `.cpp` |
| `Source/MidanCore/Public/MidanSaveGame.h` + `.cpp` |

Layout, bindings, styling and accessibility from `docs/ART_DIRECTION.md` §8:

- RPM strip **adjacent to the gear readout**, not floating across the frame (§8.2
  correction 1) — shift point and gear are read together.
- Minimap **generated from `AMidanTrackSpline`** sampled at fixed arc length, never an
  authored texture (§8.2 correction 2).
- **No backdrop blur** — a real per-frame GPU cost for a subtle effect.
- Monospace numerics throughout; proportional digits jitter as values change.
- 5% safe margin, HUD scale slider 0.75×–1.5×, minimal-HUD toggle, photo-mode toggle.
- Colourblind-safe delta indication: sign and arrow, **not colour alone**.

**Gate:** screenshot descriptions and the full settings schema. Stop.

---

## Phase 8 — Rendering & performance

**Creates:**

| Path |
|---|
| `Source/MidanTelemetry/Public/MidanHotLapReplay.h` + `.cpp` |
| `Source/MidanEditorTools/Public/MidanPerfCaptureLibrary.h` + `.cpp` |
| `Source/MidanEditorTools/Public/MidanBuildHLODCommandlet.h` + `.cpp` |
| `Tools/analysis/perf_report.py` |
| `Tools/editor_python/capture_perf_baseline.py` |

**Modifies:** `Config/DefaultScalability.ini` (four populated tiers) ·
`Config/DefaultEngine.ini` (Lumen software tracing + trace distance clamp, VSM
`ResolutionLodBiasDirectional`, TSR history settings) · `docs/PERFORMANCE_BUDGET.md`
(measured columns and provenance).

`perf_report.py` parses an Unreal Insights trace and emits the §8 template from
`docs/PERFORMANCE_BUDGET.md` with pass/fail per line.

**Gate:** run the profiling harness. Print the measured table with pass/fail per line and
all five provenance fields. **Do not proceed if a hard gate fails — fix it first.**

---

## Phase 9 — Telemetry & analysis

**Creates:**

| Path |
|---|
| `Source/MidanCore/Public/MidanTelemetryInterfaces.h` |
| `Source/MidanTelemetry/Public/MidanTelemetryFrame.h` |
| `Source/MidanTelemetry/Public/MidanTelemetryRingBuffer.h` + `.cpp` |
| `Source/MidanTelemetry/Public/MidanTelemetrySubsystem.h` + `.cpp` |
| `Source/MidanTelemetry/Public/MidanTelemetryWriter.h` + `.cpp` |
| `Source/MidanTelemetry/Private/MidanTelemetryFlushTask.h` + `.cpp` |
| `Source/MidanTelemetry/Public/MidanGhostData.h` |
| `Source/MidanTelemetry/Public/MidanGhostRecorder.h` + `.cpp` |
| `Source/MidanTelemetry/Public/MidanGhostPlayer.h` + `.cpp` |
| `Source/MidanTelemetry/Private/Tests/TelemetryRoundTripSpec.cpp` |
| `Tools/analysis/telemetry_report.py` |

Six chart types: speed trace vs distance · throttle/brake overlay · racing-line deviation
heat map · slip-angle distribution per corner · sector-time consistency · **player-vs-AI
delta trace**.

Reporting discipline: every chart carries its sample count, laps are only compared within
matched conditions, and no metric appears without its denominator.

**Gate:** capture one lap, print the report, verify zero game-thread I/O. Stop.

---

## Phase 10 — Build, CI & ship

**Creates:**

| Path |
|---|
| `Tools/build/build.py` | wraps UAT `BuildCookRun`; `--platform Mac\|Win64` |
| `Tools/build/package_shipping.py` |
| `Tools/build/generate_pso_cache.py` |
| `Tools/build/verify_build.py` |
| `Tools/build/upload_itch.py` |
| `Source/MidanTests/MidanTests.Build.cs` + functional tests |
| `.github/workflows/ci.yml` |
| `docs/BUILD_RUNBOOK.md` |

**Modifies:** `README.md` — measured performance table, controls card, minimum specs,
known issues, asset licence attributions.

`verify_build.py` hard gates: executable exists and reaches the main menu within N seconds
(headless smoke test) · pak integrity · build size within budget · no Development-only
content cooked · manifest matches git SHA · **no editor-only module referenced**.

CI is a self-hosted runner. Per assumption A4, the Mac lane is active now and the Win64
lane is gated on runner availability. Keep the DDC shared and warm — a cold DDC turns a
10-minute build into 90.

**Gate:** produce a Shipping build, run verification, report build size, launch time, and
hitch count **before and after** PSO caching. Final file summary.
