# Midan — Manual Steps

Everything in this file requires the Unreal Editor, Blender, an Epic account, or human
taste. **Claude Code cannot do any of it.** This document grows one section per phase.

The list should stay complete enough that someone other than you could reproduce the build.

---

## Phase 0 — Architecture

Phase 0 produced documentation only. There is no editor work. But four host-setup items
**block Phase 1**, and one of them changes numbers in `docs/PERFORMANCE_BUDGET.md`.

### 0.1 Install Unreal Engine 5.8 — blocks the Phase 1 compile gate

**The launcher is already installed; the engine is not.** Verified:
`/Applications/Epic Games Launcher.app` exists, but its
`LauncherInstalled.dat` has an empty `InstallationList`, and there is no
`/Users/Shared/Epic Games/UE_*` or `/Applications/UE_*`. So this is one step, not two.

1. Open the **Epic Games Launcher** and sign in.
2. Unreal Engine → Library → **install 5.8**.
3. Confirm Xcode is the selected toolchain: `xcode-select -p` should return
   `/Applications/Xcode.app/Contents/Developer`. It currently does.
4. After Phase 1 creates `Midan.uproject`, confirm its `"EngineAssociation": "5.8"`
   resolves — right-click the `.uproject` and check "Switch Unreal Engine version" shows
   5.8 already selected.

**Disk:** budget 250 GB free. UE5 plus DDC, Intermediate, and builds is enormous.

### 0.2 Enable the Python Editor Script Plugin — blocks Phase 2

Every script in `Tools/editor_python/` depends on it, starting with
`validate_vehicle_data.py` at Phase 2.

Editor → **Edit → Plugins** → search "Python Editor Script Plugin" → tick **Enabled** →
restart the editor. Also enable **Editor Scripting Utilities** while you are there.

Verify: Window → Developer Tools → Output Log, switch the console dropdown to **Python**,
and run `import unreal; print(unreal.SystemLibrary.get_engine_version())`.

### 0.3 Verify Metal feature parity — do this before Phase 8 locks any number

`docs/ART_DIRECTION.md` §7.4 requires this and it is not optional. Check Epic's current
platform documentation for **UE 5.8 on Apple Silicon / Metal** and confirm the state of:

| Feature | What to confirm | If weaker than assumed |
|---|---|---|
| **Nanite** | Supported and performant on Metal for the static track geometry | Fall back to traditional LODs for the road and rock kit — decide now, not after art is placed |
| **Lumen, software tracing** | Supported; note any quality-tier restrictions | Consider baked lighting for the static canyon with Lumen only for the vehicles |
| **Virtual Shadow Maps** | Supported; check the page-pool behaviour with a low directional light | Cascaded shadow maps with a tuned distance, accepting softer contact shadows |
| **TSR** | Supported at the intended screen percentage | FSR or plain TAA, and re-check wheel ghosting |

Record what you find in `docs/PERFORMANCE_BUDGET.md` §6.4. If any path is materially
weaker, **take the cheaper option from the start** — discovering it after six weeks of
tuning is the expensive version of this conversation.

### 0.4 Trademark-search the three vehicle names — before any shipped build

**Names are chosen** — **Sahm**, **Raqs**, **Hajar** — and `docs/VEHICLE_SPEC.md` §Naming
records the scheme and rationale. No longer blocks Phase 2.

**What still needs you:** run each name through a **trademark register for automotive
classes**. They were checked against manufacturers and models known to Claude and no
collision was found, but that is not clearance — Claude cannot search a register.

| Name | Checked against | Result |
|---|---|---|
| Sahm | Car marques and models | No automotive use found. A German glassware company exists; not automotive. |
| Raqs | Car marques and models | No automotive use found. |
| Hajar | Car marques and models | No automotive use found. A mountain range and a personal name. |

Deliberately avoided: **wind names.** That space is heavily occupied — Maserati has used
Khamsin, Shamal, Bora and Merak, Volkswagen has Scirocco, and Jeep has a Sahara trim. It is
the fastest route to an accidental collision for exactly the kind of car this project builds.

A five-minute check, and it is the difference between an original name and an expensive one.
If you also want a fictional **marque** name, it needs the same check and carries more risk,
because manufacturer marks are defended far more aggressively than model names.

### 0.5 Already verified — no action needed

| Item | State |
|---|---|
| Git LFS | 3.7.1 installed; the repo's `filter.lfs.clean` is configured. No retrofit needed |
| `.gitattributes` | Seven LFS rules present (`uasset`, `umap`, `fbx`, `wav`, `png`, `tga`, `exr`). Phase 1 adds eight more |
| `.gitignore` | Correctly excludes `Binaries/ Intermediate/ Saved/ DerivedDataCache/ Build/` |
| Blender | Installed (`/Applications/Blender.app`) |
| Xcode | Installed and selected |
| Python | 3.13.5, for the `Tools/` scripts |

### 0.6 Deferred — flagged now so it is not a surprise later

- **Windows shipping build.** The master prompt requires a Windows Shipping executable and
  a self-hosted Windows CI runner. You are developing on Apple Silicon. Phases 1–9 are
  Mac-first; Phase 10's scripts are platform-parameterised so `--platform Win64` works once
  a Windows machine or cloud runner exists. See assumption A4.
- **PSO cache hitch numbers.** `ShaderPipelineCacheTools` is a Windows-oriented toolchain
  path, so the before/after hitch counts the README needs may not be obtainable until the
  Win64 lane exists. See assumption A19.
- **The 40% that is taste.** Roughly six of your twelve weeks are manual: modelling three
  original vehicles, authoring car paint / glass / tail-light materials, sculpting the
  landscape, placing the road spline and its banking, the asphalt master material and crack
  decal library, the lighting rig, the post-process grade, the LUT, the engine audio
  layers, and tuning every curve that ships as a stub. Each gets its own section in this
  file as its phase arrives.

---

## Phase 1 — Project scaffold

The scaffold is complete on disk. Four items need you, and the first is the Phase 1 gate
itself.

### 1.1 Run the compile gate — I could not

The gate for Phase 1 is "confirm `Development Editor` compiles clean". **This has not been
verified, because no engine is installed** (§0.1). Once 5.8 is in place:

Generate the Xcode project files:

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh" -project="$PWD/Midan.uproject" -game -engine
```

Then build the editor target:

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" MidanEditor Mac Development -project="$PWD/Midan.uproject"
```

Adjust the path if the launcher installed elsewhere. **Expect a first-compile error list**
— per `docs/ASSUMPTIONS.md` A3, some 5.8 API signatures were written against thinner
knowledge than 5.5/5.6. Paste the output and I will correct it. The likely candidates, in
order:

| Candidate | If it fails |
|---|---|
| `BuildSettingsVersion.V7` | Copied from your `ProjectEmber` 5.8 target, so it should be right |
| `ChaosVehicles` module name | May need `ChaosVehiclesCore` or `Chaos` alongside it |
| `DataValidation` plugin | May be enabled by default, in which case the explicit entry is harmless |
| `Metasound` plugin name | Capitalisation varies between engine versions |
| `SF_METAL_SM6` in `DefaultEngine.ini` | The Metal shader format name changes between releases |

### 1.2 Set the default maps — after the first map exists

`Config/DefaultEngine.ini` deliberately leaves `GameDefaultMap` and `EditorStartupMap`
unset. Pointing them at a map that does not exist makes the editor error on launch.

Once you create the track map at Phase 5, add to `[/Script/EngineSettings.GameMapsSettings]`:

```ini
GameDefaultMap=/Game/Midan/Maps/L_MidanCircuit
EditorStartupMap=/Game/Midan/Maps/L_MidanCircuit
```

Create the map as a **World Partition** level with a runtime grid sized to the 3 km loop.
Per `docs/PERFORMANCE_BUDGET.md` §7, streaming distance must exceed 250 m of travel-time
headroom — at 300 km/h the car covers 83 m/s.

### 1.3 Author the Enhanced Input assets

`Config/DefaultInput.ini` declares the Enhanced Input classes but no actions — input
actions and mapping contexts are UAssets and cannot be created from code. Author these in
`Content/Midan/Input/`, with exactly these names, so `UMidanInputConfigDataAsset` can
reference them at Phase 3:

| Asset | Type | Value type | Notes |
|---|---|---|---|
| `IA_Throttle` | `UInputAction` | Axis1D | Trigger + key, `0..1` |
| `IA_Brake` | `UInputAction` | Axis1D | `0..1` |
| `IA_Steer` | `UInputAction` | Axis1D | `-1..1`. **Do not add response curve modifiers here** — shaping lives in `FVehicleSteeringConfig` |
| `IA_Handbrake` | `UInputAction` | Digital | |
| `IA_ShiftUp` | `UInputAction` | Digital | |
| `IA_ShiftDown` | `UInputAction` | Digital | |
| `IA_LookBack` | `UInputAction` | Digital | |
| `IA_CameraMode` | `UInputAction` | Digital | Cycles the four modes |
| `IA_Respawn` | `UInputAction` | Digital | |
| `IA_Pause` | `UInputAction` | Digital | |
| `IMC_MidanDriving` | `UInputMappingContext` | — | Maps all of the above for keyboard and gamepad |

**Keep modifiers off the input actions.** Deadzone belongs in `DefaultInput.ini` (already
set); response shaping belongs in the vehicle Data Asset. Putting a curve on the
`UInputAction` splits handling tuning across two places, and one of them is not diffable
as a number.

### 1.4 Verify Git LFS catches the first binary commit

The scaffold added no binary assets, so LFS is still untested in practice. When you commit
the first `.uasset`, confirm it became a pointer rather than raw bytes:

```bash
git lfs ls-files
```

An empty result after committing a binary means the filter did not run, and the fix is much
cheaper before history accumulates than after.

---

## Phase 2 — Vehicle data architecture

The schema and every validation rule are in code. What remains is authoring the assets,
and the validator will tell you precisely what is wrong with each one as you go.

### 2.1 Create the content directories

The asset manager scan paths in `Config/DefaultGame.ini` expect these, and a missing scan
directory logs a harmless warning until it exists:

```
Content/Midan/Vehicles/
Content/Midan/Surfaces/
Content/Midan/Input/
```

### 2.2 Author the surface response asset — do this first

Everything else depends on it. Create a `USurfaceResponseDataAsset` at
`Content/Midan/Surfaces/DA_SurfaceResponse`, with one row per surface. The
`PhysicalSurface` values must match the `SurfaceTypeN` order in
`Config/DefaultEngine.ini` — **that list is append-only**, because reordering it silently
remaps every physical material in `Content/` and turns tarmac into gravel with no error
anywhere.

| SurfaceTag | PhysicalSurface | Friction | Rolling res. | Roughness | On-track? |
|---|---|---|---|---|---|
| `Surface.Tarmac` | SurfaceType1 | **1.0** | 0.01 | 0.05 | yes |
| `Surface.Kerb` | SurfaceType2 | 0.95 | 0.02 | 0.85 | yes |
| `Surface.Gravel` | SurfaceType3 | 0.62 | 0.09 | 0.70 | **no** |
| `Surface.Grass` | SurfaceType4 | 0.55 | 0.12 | 0.35 | **no** |
| `Surface.Sand` | SurfaceType5 | 0.48 | 0.18 | 0.45 | **no** |
| `Surface.Wet` | SurfaceType6 | 0.72 | 0.02 | 0.05 | yes |

Tarmac must stay at 1.0 — every other surface is authored relative to it, so changing it
rescales the whole table implicitly. To make all cars grippier, raise the tyre
`FrictionForceMultiplier` instead. The validator warns if you drift from this.

Keep the gravel/tarmac spread wide. If gravel is above about 0.9 of tarmac, surface choice
stops mattering and the rally car loses the character it exists for — the validator warns
on that too.

Then create the six physical material assets (`PM_Tarmac`, `PM_Kerb`, …) and set each one's
Surface Type to match. Assign them to the road and terrain materials at Phase 5.

### 2.3 Author the three vehicle setup assets

One `UVehicleSetupDataAsset` per car in `Content/Midan/Vehicles/`. Names come from your
choice in §0.4; the class tags are fixed.

Starting points from `docs/VEHICLE_SPEC.md` — these are **first drafts to tune by feel**,
not final values. Budget three weeks for the tuning; no table gets it right on paper.

| Field | Hypercar | GT | Rally |
|---|---|---|---|
| `VehicleClass` | `Vehicle.Class.Hyper` | `Vehicle.Class.GT` | `Vehicle.Class.Rally` |
| `Mass.MassKg` | 1400 | 1550 | 1250 |
| `Mass.CentreOfMassOffset.Z` | −12 | −8 | **+5** (deliberately high) |
| `Mass.InertiaTensorScale` | 1.0 | 1.05 | 0.9 |
| `Drivetrain.Layout` | AllWheelDrive | **RearWheelDrive** | AllWheelDrive |
| `Drivetrain.FrontTorqueSplit` | 0.35 | — | 0.5 |
| `Drivetrain.RearDifferentialLock` | 0.6 | **0.75** | 0.5 |
| `Powertrain.MaxRPM` | 9000 | 7500 | 6500 |
| Torque curve character | **peaky, strong top end** | **flat, wide** | **torquey low end** |
| Peak torque (Nm) | ~750 @ 7000 | ~540 @ 5000 | ~430 @ 3000 |
| `Tyres.Front.FrictionForceMultiplier` | 2.35 | **2.20** | 1.95 |
| `Tyres.Rear.FrictionForceMultiplier` | 2.45 | **2.02** | 2.00 |
| Resulting balance | 0.96 (mild understeer) | **1.09 (oversteer)** | 0.98 |
| `Suspension` travel (raise+drop, cm) | 5+5 | 7+7 | **14+14** |
| `Suspension.DampingRatio` | 0.85 | 0.70 | **0.55** |
| `Aero.FrontLiftCoefficient` | 1.1 | 0.5 | 0.1 |
| `Aero.RearLiftCoefficient` | 1.7 | 0.8 | 0.15 |

The three must **argue with each other**. Three cars that feel the same is a worse
portfolio than one car that feels great, and the levers that differentiate them fastest are
drivetrain layout, the front/rear friction ratio, torque curve shape, and suspension travel.

### 2.4 Author the mandatory curves

The validator **errors** on a missing steering curve and on a torque curve that does not
span the rev range. Both are silent failures at runtime, which is why they error rather than
warn.

**`Steering.SteeringCurve`** — input km/h, output steer multiplier 0..1. Must never rise.

| Speed | Multiplier |
|---|---|
| 0 | 1.00 |
| 60 | 0.70 |
| 120 | 0.45 |
| 200 | 0.30 |
| 300+ | 0.20 |

**`Steering.KeyboardShapingCurve`** — input seconds held, output magnitude 0..1. Ramp over
roughly 150ms; do not copy the analogue curve.

| Time held | Magnitude |
|---|---|
| 0.00 | 0.00 |
| 0.05 | 0.35 |
| 0.15 | 1.00 |

**`Powertrain.TorqueCurve`** — input RPM, output **absolute Nm** (not normalised — the
applier handles the conversion, see `docs/ASSUMPTIONS.md` A22). Domain must cover idle to
`MaxRPM`.

**`Tyres.*.LateralSlipGraph`** — input slip angle in degrees, output force coefficient. The
shape is what makes slides catchable: a rounded peak with a gentle fall-off gives
progressive slides, a sharp peak with a steep fall-off gives a car that snaps. For the GT
car the gentle fall-off matters more than the absolute grip level.

### 2.5 Set the axle load ratios to sum to 1.0

`Suspension.Front.WheelLoadRatio + Rear.WheelLoadRatio` must equal 1.0 — this is an
**error**, not a warning, because every tyre value is calibrated against axle load and a
partial distribution makes all of them wrong against a false baseline. Roughly 0.45/0.55
front/rear for a mid-engine layout, 0.52/0.48 for front-engine.

### 2.6 Run the validators

In-editor: **Tools → Validate Data**, or right-click an asset → **Asset Actions → Validate
Assets**. Headless, and the form CI uses:

```bash
"<UE>/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/Midan.uproject" -run=DataValidation
```

Or via the Python tool, which exits non-zero on any error:

```bash
"<UE>/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/Midan.uproject" -run=pythonscript -script="$PWD/Tools/editor_python/validate_vehicle_data.py"
```

Warnings never fail the run. A high centre of mass on the rally car is intentional, and a
validator that fights the design gets switched off.

### 2.7 Verify the Chaos field names in the applier

`Source/MidanVehicle/Private/VehicleSetupApplier.cpp` has every Chaos assignment marked
`// API VERIFY`. Those spellings were written without access to UE 5.8 headers. Once the
engine is installed, check them against
`Engine/Plugins/Runtime/ChaosVehicles/Source/ChaosVehicles/Public/` and paste any compile
errors — see `docs/ASSUMPTIONS.md` A21 for the three known version-sensitive spots.

---

## Phase 4 — Feel layer

The components are written. **None of them does anything until a human authors the
curves.** That is not a defect — it is the design: every feel value is a curve in
`UVehicleFeelDataAsset`, and code that invented defaults would be a second source of truth
competing with the asset.

This section is the Phase 4 gate deliverable: **every curve you must author, its domain,
its recommended shape, and what going wrong feels like.**

### 4.1 How to author these

1. **Create one `UVehicleFeelDataAsset` per car** in `Content/Midan/Vehicles/`, and point
   each `UVehicleSetupDataAsset::Feel` at its own. The three cars must NOT share one — the
   camera is a large part of why Sahm feels different from Hajar.
2. **Author Deep Night first** if you are also tuning lighting, so the camera is judged
   against the hero preset.
3. **Change one curve at a time.** Two simultaneous changes teach you nothing about either.
4. **Overshoot deliberately, then come back.** Push each value until it is obviously wrong
   in both directions to find the usable range, then bisect. Starting from the recommended
   value and nudging finds a local optimum and stays there.
5. **Test at more than one frame rate.** All damping here is frame-rate independent by
   construction, and this is how you confirm it.
6. **Take breaks.** Sensitivity to feel degrades within about twenty minutes, and the
   version you converge on while numb will be over-tuned.

### 4.2 Camera curves — per mode, four modes per car

X axis is **normalised speed 0..1**, where 1.0 is `SpeedNormalisationKmh` (default 320).
All four modes need all three curves.

| Curve | Domain → Range | Recommended shape | What wrong feels like |
|---|---|---|---|
| **`FOVOffsetBySpeed`** | 0..1 → 0..28 | Flat to ~0.2, then rising, steepest 0.4–0.8, flattening at 1.0. ART_DIRECTION §5 wants 72° base reaching 95–100°, so end at **23–28**. | **The single biggest speed lever.** Too flat and 250 km/h feels like 80. Too steep and the car appears to shrink; a linear ramp makes low speed feel sluggish. |
| **`LocationLagBySpeed`** | 0..1 → 8..12 | Gently *falling* — more lag at speed. | Too high (stiff) and the car has no mass. Too low and the camera swims and induces motion sickness. |
| **`RotationLagBySpeed`** | 0..1 → 6..9 | Gently falling, always **below** the location lag. | Rotation lag above location lag makes the camera point away from travel through a corner, which reads as broken. |

Per-mode scalar starting points, from ART_DIRECTION §5:

| Mode | `ArmLengthCm` | `HeightCm` | `PitchDegrees` | `LookAheadYawDegrees` | `SlipYawDegrees` | `VerticalDamping` |
|---|---|---|---|---|---|---|
| ChaseFar | 600–650 | 200–220 | −7 to −9 | 4 | 6 | 0.7 |
| ChaseNear | 450–550 | 170–190 | −6 to −7 | 3 | 5 | 0.7 |
| Bonnet | 150–200 *(forward offset, not a boom)* | 110–130 | −2 to 0 | **0** *(ignored)* | 4 | **0.85** |
| Cockpit | −20 to 20 | 105–120 | 0 to 2 | **0** *(ignored)* | 4 | **0.9** |

Vertical damping is **higher** on the rigid modes, not lower: a rigid camera takes the full
suspension chatter directly, and unfiltered chatter at head height is genuinely nauseating.

### 4.3 Shake and rumble

| Curve | Domain → Range | Recommended shape | What wrong feels like |
|---|---|---|---|
| **`ImpactShakeByImpulse`** | 0..1 *(normalised by `FullScaleImpactImpulse`)* → 0..6 degrees | Rising, slightly concave. Near-zero below 0.1 so scrapes do not shake. | Linear-from-zero makes every barrier brush shake the screen. Too high at 1.0 and a crash hides the road at the moment you most need to see it. |
| **`SurfaceRumbleByRoughness`** | 0..1 → 0..0.8 degrees | Near-linear, gentle. **Keep it small.** | Above ~1.0 degree it stops reading as texture and starts reading as a broken camera. |

### 4.4 Audio curves

| Curve | Domain → Range | Recommended shape | What wrong feels like |
|---|---|---|---|
| **`WindGainBySpeed`** | 0..1 → 0..1 | Silent to ~0.15, then rising roughly with the square of speed. | Linear from zero puts wind noise at a standstill. Too loud at the top and it masks the engine, which is the sound players actually want. |
| **`TyreScrubGainBySlip`** | 0..1 → 0..1 | Near-zero below the tyre's slip threshold, then rising sharply, flattening by 0.6. | A gentle ramp makes the car sound like it is sliding when it is gripping, which destroys the audio cue entirely. |

Evaluated **per axle** at runtime, so understeer and oversteer sound different. That is the
clearest cue the player has about which end is letting go — do not author it flat.

### 4.5 FX and post curves

| Curve | Domain → Range | Recommended shape | What wrong feels like |
|---|---|---|---|
| **`TyreSmokeRateBySlip`** | 0..1 → 0..(system max) | Zero below ~0.25, then rising steeply. | Smoke at low slip makes the car look permanently out of control. |
| **`ChromaticAberrationBySpeed`** | 0..1 → 0..0.4 | Zero to ~0.3 speed, then gentle rise. ART_DIRECTION §6: **0.2–0.4 max.** | Perceptible at rest reads as a broken lens. If a reviewer can name the effect, it is too strong. |
| **`VignetteBySpeed`** | 0..1 → 0.3..0.4 | Nearly flat; slight rise with speed. | Above 0.5 the frame looks like a telescope. |
| **`MotionBlurBySpeed`** | 0..1 → 0..0.5 | Rising from ~0.1 speed. ART_DIRECTION §6: **0.4–0.5 object blur.** | Too low and speed does not read. **Never author this to zero** — it is feel, not fidelity (§7.3). The player's slider handles personal preference. |

### 4.6 Haptic curve

| Curve | Domain → Range | Recommended shape | What wrong feels like |
|---|---|---|---|
| **`HapticAmplitudeByImpulse`** | 0..1 → 0..1 | **Match `ImpactShakeByImpulse`'s shape.** | If the two disagree, a collision that looks minor feels severe and the player stops trusting both channels. |

### 4.7 Assets you must author or source

| Asset | Notes |
|---|---|
| `EngineSound` — MetaSound | **Minimum four RPM layers**, in **separate on-load and off-load sets**. Parameters pushed by code: `RPM`, `Load`, `Speed`, `Gear`, `Shifting`. Single-axis RPM blending produces a whine; the load axis is what makes lifting off mean something. |
| `TyreScrubSound` — MetaSound | Parameters: `SlipFront`, `SlipRear`, `SurfaceRoughness`. Gravel and tarmac should be different *sounds*, not one sound at different volumes. |
| `WindSound` | Parameter: `Speed`. |
| `ImpactSound`, `BackfireSound` | One-shots. Code applies per-instance pitch variation. |
| `TyreSmokeSystem` — Niagara | Must expose a **`SpawnRate`** float user parameter. Code sets rate; the system never self-activates. |
| `ExhaustBackfireSystem` — Niagara | One-shot burst. |
| `SkidDecalMaterial` | Deferred decal. Pool size defaults to 96 per vehicle. |
| 5 × `UForceFeedbackEffect` | Idle, surface, slip (**looping**); impact, kerb (**one-shot**). |

### 4.8 Known gaps at this gate

- **Nothing is verified.** No engine is installed, so none of this has been compiled or
  driven. Every Chaos and force-feedback call is marked `API VERIFY` — see
  `docs/ASSUMPTIONS.md` A27 and A30.
- **Looping force-feedback amplitude modulation is the least certain API** in the phase.
  The channel design (two continuous, two transient, all sharing the impulse curve family)
  is what matters and does not change with the accessor's spelling.
- **The three continuous haptic loops are never spawned yet.** `IdleRumble`,
  `SurfaceRumble` and `SlipRumble` are declared and modulated but not created, pending the
  5.8 API check above. Transient impacts and kerb strikes work through
  `ClientPlayForceFeedback` and do not depend on it.
- **Budget real time for the tuning itself.** Code delivers every system and every curve
  correctly wired; the result will still feel wrong until a human sits down and tunes it.
  Studios employ specialists for this.
