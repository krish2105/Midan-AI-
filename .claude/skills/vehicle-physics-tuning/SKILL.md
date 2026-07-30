---
name: vehicle-physics-tuning
description: Use when configuring or debugging vehicle handling in Chaos Vehicles — mass and centre of mass, torque curves, gear ratios, differentials, suspension springs and damping, tyre friction and slip graphs, steering curves, downforce and drag, surface friction, or substepping and physics stability. Trigger for "the car flips", "it feels floaty", "understeer", "oversteer", "tune the suspension", "steering feels wrong", "downforce", "wheel setup", "physics substepping". Do NOT trigger for vehicle art, rigging topology, or non-vehicle physics.
---

# Vehicle Physics Tuning (Chaos)

Handling is where racing games are won and lost, and it is mostly not a code problem. This
skill covers what each parameter does to the *feel* of the car, which mistakes are
structural rather than numerical, and how to keep the data pipeline that makes tuning
tractable.

**The prerequisite:** every value here lives in a Data Asset, never in C++. If a value a
human tunes by feel has been inlined into code, stop and fix that first — otherwise you
cannot diff a handling change, validate it in CI, or sweep it programmatically, and each
tuning iteration costs a recompile.

---

## 1. Get stability right first, or fight it for weeks

Configure this before touching a single handling value.

```ini
[/Script/Engine.PhysicsSettings]
bSubstepping=True
MaxSubstepDeltaTime=0.008333        ; 120 Hz
MaxSubsteps=6
bTickPhysicsAsync=True
AsyncFixedTimeStepSize=0.008333
```

Then hold these rules:

- **All force application in the async physics callback**, never in `Tick`. Force applied
  at frame rate against physics running at 120 Hz produces frame-rate-dependent handling —
  the car behaves differently at 30 fps and 144 fps, and you will chase that for a week
  before finding it.
- **Chassis collision is a convex hull**, not complex collision. Wheels are spheres or
  capsules.
- **Never scale a vehicle actor at runtime.**
- **Verify bone weighting on the physics asset.** Bad bone weighting is the single most
  common cause of a car that flips unpredictably. Before tuning centre of mass to fix a
  flip, check the rig — you are usually treating a symptom.
- **Real-world metric scale.** A car is roughly 4.5 m long, 1.9 m wide, 1.3 m tall. Wrong
  scale invalidates every physics number and you will not notice until week 6.

Symptom-to-cause, for the failures that are structural rather than numerical:

| Symptom | Usual cause |
|---|---|
| Flips on a kerb strike | Bone weighting, or complex collision on the chassis |
| Handling differs by frame rate | Force applied in `Tick` instead of the physics callback |
| Twitchy at low speed, vague at high | Raw input mapped to steer angle with no speed curve |
| Sinks or floats through the ground | Wheel radius mismatched to the mesh, or suspension offset wrong |
| Jitters at rest | Substepping disabled, or spring rate far too high for the mass |

---

## 2. Mass and inertia

| Field | Handling consequence |
|---|---|
| `Mass` (kg) | Everything downstream. Set it realistically **first**, then tune elsewhere. Fudging mass to fix a symptom breaks every other parameter's meaning. |
| `CentreOfMassOffset` (Z) | **A design dial, not a bug fix.** Lowering it artificially stabilises the car and kills body movement. For a rally car that removes exactly the character you want. Expose it; never quietly lower it to stop a flip. |
| `InertiaTensorScale` | Rotational willingness. Raise for reluctance and stability, lower for agility. The cheapest way to make two cars of similar mass feel different. |

A validator should **warn** on an unusually high centre of mass, never error — the whole
point is that a high COM is sometimes intentional.

---

## 3. Powertrain — the difference between fast and *feeling* fast

| Field | Consequence |
|---|---|
| `TorqueCurve` (Nm vs RPM) | The most expressive parameter you have. **Peaky curves feel dramatic; flat curves feel fast but dull.** A car that pulls hard at 6,000 rpm is memorable; one with identical lap time and a flat curve is not. |
| `MaxRPM`, `EngineIdleRPM` | The usable band. The curve must cover it — a curve whose domain stops short of `MaxRPM` produces a car that mysteriously dies at the top end. |
| `EngineBrakeEffect` | Off-throttle deceleration. A large and underrated feel contributor — it is what makes lifting off *mean* something. |
| `GearRatios[]`, `FinalRatio` | Gear spacing controls **how often the player feels an event**. Close ratios give frequent shifts and a sense of busyness; long ratios give fewer, bigger moments. |
| `ChangeUpTime` / `ChangeDownTime` | Shift punch. Sub-0.15 s reads as a modern dual-clutch. Above ~0.4 s reads as an old automatic, which is a valid choice if you mean it. |

Validate: at least two gears, monotonically decreasing ratios, positive final drive, torque
curve domain covering `[EngineIdleRPM, MaxRPM]`, no negative torque in range.

---

## 4. Drivetrain

`DifferentialType` (AllWheelDrive / FrontWheelDrive / RearWheelDrive), `FrontRearSplit`,
and the front and rear left-right splits.

- **RWD + high torque = oversteer character.** The drift car.
- **AWD = stability and traction off the line.** The point-and-shoot car.
- **FWD** pulls the nose wide under power — rarely what a racing slice wants, but distinctive.

Torque split is a stronger character lever than most people expect: the same chassis at
40/60 and 60/40 front-rear feels like two different cars.

---

## 5. Suspension — where amateur racing games fail

Per axle: `SuspensionMaxRaise`, `SuspensionMaxDrop`, `SpringRate`, `SpringPreload`,
`SuspensionDampingRatio`, `SuspensionForceOffset`, `WheelLoadRatio`.

**Body roll is how the player reads grip.** The player cannot see a friction coefficient;
they see the car lean, and they infer the limit from it. Suppress the lean and you have
removed their instrument.

| Mistake | Result |
|---|---|
| Over-damping | Fast car that feels **dead**. Numbers look good, nobody enjoys driving it. This is the more common failure, because it is what you get when you "fix" instability with damping. |
| Under-damping | A boat. Oscillates after every input, feels disconnected. |
| Travel too short | Every bump is an impact; the car skates rather than absorbing. |

Rally versus GT lives almost entirely here: high travel plus visible body movement plus a
higher COM produces a car that moves around and communicates constantly. That is the
character, not a defect.

Tune damping by watching the car settle after a kerb: one clear compression and rebound,
then settled. Two or three oscillations is under-damped; no visible movement at all is
over-damped.

---

## 6. Tyres — your understeer/oversteer balance

Per axle: `FrictionForceMultiplier`, `LateralSlipGraph`, `CorneringStiffness`,
`SlipThreshold`, `SkidThreshold`.

**The front/rear friction ratio is the balance.**

| Ratio | Character |
|---|---|
| Rear friction **higher** than front | Understeer. Safe, forgiving, dull. |
| Rear friction **lower** than front | Oversteer. Exciting, expressive, punishing. |

**Expose the ratio itself as a single derived tuning value in the Data Asset**, not just the
two absolute multipliers. It is the value you actually want to sweep, and sweeping one
number beats sweeping two correlated ones.

The `LateralSlipGraph` shape controls how the limit *arrives*. A sharp peak with a steep
fall-off gives a knife-edge car that snaps. A rounded peak with a gentle fall-off gives
progressive, catchable slides — which is what "forgiving at the limit" means mechanically.
For a drift-oriented car, the gentle fall-off matters more than the absolute grip level.

---

## 7. Steering — never map input directly to angle

`SteeringCurve` (max steer angle vs forward speed) is **mandatory**. Raw stick or key input
mapped straight to steer angle is the single most common reason a vehicle "feels wrong":
full lock at 250 km/h is an instant spin, and the angle that feels right in a hairpin is
unusable on a straight.

Also:

- `SteeringInputRate` — separate rise and fall interpolation rates. Fast rise with slower
  fall feels responsive but stable; the reverse feels sluggish and nervous.
- `AckermannAccuracy` — geometric correctness of the inner/outer wheel angles. Matters more
  at low speed and full lock than anywhere else.
- **A separate keyboard shaping curve.** Binary keys through a linear map is a distinct
  failure from gamepad tuning and needs its own curve, not a scaled version of the analogue
  one. Ramp in over 100–200 ms rather than jumping to the curve value.

Validate that the steering curve's domain covers zero to expected top speed and is
monotonically non-increasing.

---

## 8. Aero — a cheap senior touch

Chaos does not give usable downforce out of the box. Implement it in a component that runs
in the physics callback:

- **Drag:** `-0.5 · ρ · Cd · A · v² · v̂` at the centre of pressure
- **Downforce:** `-0.5 · ρ · Cl · A · v²` applied at **separate front and rear points**

The separate application points are the whole value of doing this. Front/rear downforce
balance shifting with speed is what makes a car feel planted at 250 km/h and nervous at 60
— it produces a speed-dependent handling personality from four numbers, which no amount of
suspension tuning can imitate.

Cheap to implement, and it is the kind of detail that reads as deliberate engineering.

---

## 9. Surfaces

Physical materials (`Tarmac`, `Kerb`, `Gravel`, `Grass`, `Sand`, `Wet`) each map to a
friction multiplier plus an audio and FX row in a data table.

**Read the physical material under each wheel. Do not use trigger volumes for off-track
detection.** Trigger volumes require authoring geometry that duplicates the track shape,
go stale the first time a corner moves, and cannot express "two wheels on gravel".

Per-wheel surface state is also what lets a surface-sensitive car exist at all: if the
right-hand wheels are on gravel and the left on tarmac, the car should pull. That is only
possible with per-wheel sampling.

---

## 10. Making cars argue with each other

Three cars that feel the same is a worse result than one car that feels great. The
differentiating levers, in rough order of effect per unit of effort:

1. **Drivetrain layout** — RWD vs AWD is instantly legible to the player
2. **Front/rear tyre friction ratio** — the understeer/oversteer character
3. **Torque curve shape** — peaky versus flat, dramatic versus relentless
4. **Suspension travel and damping** — how much the body moves and talks
5. **Centre of mass height** — how willing the car is to rotate and roll
6. **Aero balance** — whether it feels planted or nervous, and at what speed
7. **Inertia tensor scale** — rotational eagerness

Change several of these together and deliberately in opposite directions. Two cars that
differ only in power feel like the same car with a different number.

---

## 11. Checklist

- [ ] Substepping and async physics tick enabled before any handling tuning
- [ ] All force application in the physics callback, zero allocations there
- [ ] Chassis convex hull, wheels sphere/capsule, bone weighting verified
- [ ] Real-world metric scale confirmed
- [ ] Every value in a Data Asset; zero tuning floats in C++
- [ ] Torque curve domain covers the full RPM band; gear ratios monotonic
- [ ] Steering curve present, domain covers zero to top speed
- [ ] Separate keyboard input shaping curve
- [ ] Front/rear friction ratio exposed as one sweepable derived value
- [ ] Downforce applied at separate front and rear points
- [ ] Per-wheel physical material sampling; no trigger volumes for off-track
- [ ] Centre of mass warns rather than errors in validation
- [ ] The three cars differ on at least four of the levers in §10
