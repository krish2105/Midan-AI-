# MIDAN — ART DIRECTION & RENDERING SPECIFICATION
**Attach alongside `MIDAN_UE5_RACING_CLAUDE_CODE_MASTER_PROMPT.md`**
Version 1.0 · Target: 1080p / 60fps primary, 1440p stretch

---

## 0. Legal boundary — read first

The reference frame depicts a real, badged production vehicle. **This is not shippable.**

Vehicle manufacturers enforce trade dress on silhouette, grille, badge, and light signature. Licensing is a business function, not a formality — it is a large part of why AAA racing studios have dedicated teams for it.

**The rule for this project:** vehicles are original designs that borrow *design language*, never geometry. From the reference, the transferable language is:

- Wide rear haunches with a pronounced shoulder line over the rear arches
- Quad circular tail lamps (a broad automotive convention, not owned by anyone)
- High-mounted rear wing with visible end plates
- Compact greenhouse, fast rear screen
- Quad exhaust exits, centre-biased

**Forbidden:** any badge, any manufacturer name, any model name, any grille shape traceable to a specific marque, any light signature that is a recognisable brand identifier. If a car enthusiast can name it, redesign it.

Record every third-party asset and its licence in `docs/ASSET_LICENCES.md` from day one.

---

## 1. What this reference actually is, and what is achievable

The reference is a generated photographic image. Being clear about the gap prevents three months of chasing an impossible target.

| Element | Achievable in UE5 | Difficulty | Notes |
|---|---|---|---|
| Canyon rock walls | **Yes, near-identical** | Low | Megascans cliff assemblies are photogrammetry. This is the easiest win in the frame. |
| Golden-hour sky and light | **Yes** | Low | Directional light + Sky Atmosphere + volumetric fog. Mostly parameter work. |
| Asphalt with cracks and wear | **Yes** | Low-Medium | Megascans road surface + decal layer |
| Guardrails, scrub, road markings | **Yes** | Low | Kit-bash + decals + PCG scatter |
| Atmospheric depth / haze | **Yes** | Low | Exponential height fog, volumetric |
| **The car** | **Yes, but this is the work** | **High** | Original design, clean topology, correct materials, correct scale. This is the single biggest art risk. |
| Wet-looking specular road sheen | Yes | Medium | Detail normal + roughness variation, not literal wetness |
| Overall "photographic" feel | Mostly | Medium | Post stack, exposure, grain, subtle CA. Section 6. |

**Verdict: roughly 85% of this frame is reachable by one person in 12 weeks, and the environment is the easy 85%.** Budget your art time on the car.

---

## 2. Lighting setup — the frame's entire mood comes from here

Golden hour is the correct choice and not just aesthetically: low warm key light with long shadows hides geometry deficiencies, gives free rim lighting on the car, and makes modest assets read as expensive.

### 2.1 Directional light (sun)
| Parameter | Value | Why |
|---|---|---|
| Pitch | **-3° to -6°** | Just above the horizon. This is the whole look. |
| Yaw | Aligned to the track's main straight, ±10° | Sun near the vanishing point gives the backlit silhouette |
| Intensity | 3–6 lux (physical units) | Low sun is dim; let auto-exposure do the lifting |
| Temperature | **2700–3200K** | Warm. Do not go below 2500K — it reads as orange filter, not sunlight |
| Source angle | 1.5–3.0° | Softer shadow edges than default; low sun through atmosphere is not a point source |
| Cast shadows | On, Virtual Shadow Maps | See §7 for the cost warning |
| Light shafts | On, subtle | The godray through the canyon gap |

### 2.2 Sky and atmosphere
- **Sky Atmosphere** component — physically-based scattering does the pink-to-orange gradient for free. Do not paint a skybox.
- **Sky Light**, real-time capture, intensity ~1.0. This is your cool fill and it is what keeps shadows blue rather than black.
- **Volumetric Cloud** with a thin, wispy profile. Keep coverage low — the reference has streaky high cloud, not cumulus. Volumetric clouds are expensive; if they cost more than 1.0ms, replace with a cloud texture on the sky.
- **Exponential Height Fog**, volumetric enabled. Fog inscattering colour warm-tinted toward the sun. This creates the layered depth on the distant peaks and it is the cheapest atmosphere you will ever buy.

### 2.3 Colour temperature contrast (the professional touch)
The frame works because warm key and cool fill are in opposition:
- Key light: 2900K warm
- Sky light fill: naturally cool blue
- Bounce off red rock: warm secondary
- Shadow interiors: blue-violet, never neutral grey

If your scene looks flat, it is almost always because shadows have gone grey. Push sky light saturation before you touch anything else.

### 2.4 Time-of-day is locked
**One lighting condition only.** Do not build a day/night cycle. It multiplies your lighting authoring, your PSO cache coverage, and your testing surface for a vertical slice that will be viewed for 90 seconds. Lock the sun, bake what you can, and spend the time on the car.

---

## 3. Environment bill of materials

Build the track as a kit, not as unique geometry.

### 3.1 Terrain and rock
- **Landscape** for the base ground plane and broad elevation
- **Megascans cliff/rock assemblies** for the canyon walls — Nanite, no LODs needed, place as large hero pieces plus mid and small scatter
- Rock material: **triplanar projection** with a large-scale detail normal so tiling is invisible at close range
- Colour variation via a vertex-painted or world-position-driven tint mask — uniform rock colour is the fastest way to look like an asset pack

### 3.2 Road
- **Spline mesh road** from `AMidanTrackSpline` (already in the build spec)
- Base asphalt material: Megascans road scan, 2–4m tiling, with a large-scale macro variation mask breaking up repetition
- **Crack and patch decals** — a library of 8–12 decals scattered along the spline. This is what sells realism and it is nearly free
- **Road markings as decals**, never as texture in the base material. Double centre line, edge lines, and worn variants. Decals let you author the line pattern independently of road geometry
- Roughness variation is critical: uniform-roughness asphalt reads as plastic. Use a breakup mask so specular highlights pool unevenly — this creates the wet-looking sheen in the reference without any actual wetness

### 3.3 Furniture
- Guardrail: one modular mesh + post, instanced along a spline
- Sparse desert scrub via **PCG**, with a distance-from-road-spline exclusion mask so nothing grows on the tarmac
- Distance markers, occasional signage (original, no real logos)
- Keep total unique mesh count low. Repetition is fine at speed; unique geometry is not worth the time

### 3.4 Composition rule taken from the reference
**The road curves out of frame toward the light.** Design at least three points on your circuit where a corner exit points the camera at the sun with a canyon gap framing it. Those are your screenshot and trailer moments, and they should be deliberate, not discovered.

---

## 4. Vehicle art specification

This is the highest-risk deliverable. Plan it properly.

### 4.1 Modelling budget
| Item | Target |
|---|---|
| Exterior body | 80–150k triangles (Nanite handles this comfortably) |
| Wheels | 15–25k each, separate mesh, correct pivot |
| Interior | Low detail — this slice has no cockpit camera as a headline feature |
| Skeleton | Root, body, 4 wheel bones, 4 suspension bones. Named consistently. |
| Scale | **Real-world metric.** A car is ~4.5m long, ~1.9m wide, ~1.3m tall. Wrong scale breaks physics tuning and you will not notice until week 6. |

### 4.2 Materials
- **Car paint master material**: base colour, metallic flake layer, clearcoat with independent roughness. Clearcoat is what makes paint read as paint rather than plastic.
- **Glass**: proper transmission, thin-surface, with a subtle tint and interior visible through it. Fully opaque black glass is an instant tell.
- **Tail lights**: emissive with a lens material over it. The reference's quad-circle glow is emissive intensity plus bloom, not a bright texture.
- **Rubber**: high roughness, near-zero specular, with a sidewall normal map.
- **Brake discs and callipers**: separate material, metallic, with emissive-on-heat driven from brake input (cheap, and a lovely detail at night or in shadow).

### 4.3 Where to source
Sketchfab CC0 and CC-BY for a base mesh you then **modify substantially** into an original design. Do not asset-flip a recognisable car. If you cannot model, budget 2 weeks in Blender — a car is genuinely one of the harder things to model well, and doing it is itself a portfolio credential.

---

## 5. Camera specification (derived from the reference framing)

| Parameter | Value | Note |
|---|---|---|
| FOV base | **72°** | The reference is a fairly tight lens |
| FOV at top speed | **95–100°** | Curve-driven. The largest speed-sensation lever you have. |
| Spring arm length | 5.5–6.5m | Car occupies roughly the lower centre third |
| Height above car origin | 1.8–2.2m | |
| Pitch | **-6° to -9°** | Slight downward look; keeps horizon in the upper third |
| Horizon placement | Upper third | Reference puts it at ~45% height — road dominates |
| Location lag | 8–12 (speed-curved) | |
| Rotation lag | 6–9 | |
| Look-ahead yaw | ±4° from steering, damped | |
| Slip yaw | ±6° from chassis slip angle | Makes drifts read |

Full camera behaviour list is in §2 of the master prompt and the `game-feel-engineering` skill.

---

## 6. Post-process stack (the "photographic" layer)

Configure as a post-process volume, unbound, with these deviations from default. **Every value here is a Data Asset field**, not hardcoded.

| Effect | Setting | Why |
|---|---|---|
| **Exposure** | Manual or heavily clamped auto (min/max EV within ~1.5 stops) | Auto-exposure hunting on a moving camera is nauseating and destroys the golden-hour look. This is the most important line in this table. |
| **Bloom** | Convolution or standard, intensity 0.4–0.7, threshold ~1.0 | The sun glow and tail-light bleed. Restraint — over-bloom reads as amateur. |
| **Lens flare** | Off, or a single very subtle element | Anamorphic streaks read as a filter, not as a camera |
| **Chromatic aberration** | 0.2–0.4, speed-scaled | Barely perceptible at rest, noticeable at speed |
| **Vignette** | 0.3–0.4 | Focuses attention on the road centre |
| **Film grain** | 0.15–0.25 | Kills banding in sky gradients and adds photographic texture. Do not skip this. |
| **Motion blur** | Object blur 0.4–0.5, camera blur 0.15–0.25 | Speed sensation. Expose an intensity slider for accessibility. |
| **Colour grading** | Warm highlights, cool-lifted shadows, slight saturation boost, gentle S-curve contrast | This is what makes it look graded rather than raw |
| **LUT** | One custom LUT, authored last | Apply only after everything else is right. A LUT cannot fix bad lighting. |
| **Ambient occlusion** | Lumen-provided; do not stack SSAO on top | Double-darkening contact points is a common mistake |

**Sky banding warning:** large smooth gradients band badly at 8-bit. Film grain plus dithering fixes it. Check on the darkest display you can find.

---

## 7. Performance reality on a MacBook

The reference frame is expensive. Here is what to hold and what to cut.

### 7.1 Revised budget — 1080p/60 primary
| Line | Budget (ms) |
|---|---|
| Total frame | 16.6 |
| GPU total | ≤ 15.0 |
| — Base pass (Nanite) | ≤ 3.0 |
| — Lumen GI + reflections | ≤ 3.5 |
| — Virtual Shadow Maps | ≤ 2.5 |
| — Volumetric fog + clouds | ≤ 1.5 |
| — Post + TSR | ≤ 2.5 |
| — Translucency/particles | ≤ 1.0 |

### 7.2 The three costs that will bite
1. **VSM page pool with a low-angle directional light.** Long shadows across an open canyon is precisely the worst case. Clamp `r.Shadow.Virtual.ResolutionLodBiasDirectional` and measure. If it exceeds 2.5ms, raise the bias before you cut anything else.
2. **Volumetric clouds.** Beautiful, and often 2ms+. Measure them alone. If they cost more than 1.0ms, swap to a cloud texture on the sky dome — at 250km/h nobody is studying cloud detail.
3. **Lumen in a wide-open environment.** Use **software tracing**, clamp max trace distance to what the canyon actually needs, and lean on the sky light for distant fill.

### 7.3 What to cut first if you are over budget
In order: volumetric clouds → Lumen reflection quality → VSM resolution → screen percentage (TSR does the rest) → foliage density. **Never cut the film grain, the exposure clamp, or the motion blur** — those are feel, not fidelity, and cutting them costs you more than the frame time saves.

### 7.4 Mac-specific
Verify current Metal feature parity for Nanite, Lumen, and VSM against Epic's platform documentation before locking §2 and §7 — this moves between engine releases and my information may be behind. If any path is weaker than expected, choose the cheaper option *from the start* rather than after tuning.

---

## 8. HUD specification

The reference layout is good. Build exactly this, with two corrections.

### 8.1 Layout
| Element | Position | Data binding |
|---|---|---|
| RPM strip | Bottom-left, above speed cluster, ~264px wide | Normalised RPM from the movement component. Green → amber at 80% → red at 92%. |
| Speed | Bottom-left panel | `GetForwardSpeed()` converted to km/h, monospace, 40px |
| Gear | Bottom-left, adjacent panel | Current gear. Colour-shift to amber near the shift point. |
| Position / Lap | Top-right | 10Hz arc-length position calc; lap from `UMidanLapTimingSubsystem` |
| Sector delta | Top-right, below position | Live delta vs personal best sector. Green negative, red positive. |
| Minimap | Bottom-right | Generated from the track spline, not an authored texture |

### 8.2 Two corrections to the reference
1. **Move the RPM strip adjacent to the gear readout**, not floating across the frame. Shift point and gear are read together; separating them costs the player a saccade at exactly the wrong moment.
2. **The minimap must be spline-generated.** Draw it from `AMidanTrackSpline` sampled at fixed arc length, so it stays correct when you change the track. An authored minimap texture goes stale the first time you move a corner.

### 8.3 HUD styling
- Panels: dark translucent, ~72% opacity, 6px corner radius, no border
- **No backdrop blur.** It is a real GPU cost every frame for a subtle effect. The reference's blur is generated, not rendered.
- Monospace for all numerics — proportional digits jitter as values change and it looks cheap
- Label text at 12px, uppercase, muted; values large and high-contrast
- Fade non-critical elements (minimap, sector) during the countdown and results

### 8.4 Safe zones and accessibility
Keep all HUD elements within a 5% safe margin. Provide a HUD scale slider (0.75×–1.5×), a minimal-HUD toggle, and a photo-mode toggle that hides everything. Colourblind-safe delta indication: use sign and arrow, not colour alone.

---

## 9. Screenshot discipline

Your portfolio will be judged on three or four still frames before anyone plays it. Author them deliberately:

- Build a **photo mode** — free camera, HUD toggle, FOV control, time-scale pause. Two days of work, and every screenshot you ever take depends on it.
- Compose to the reference's rules: horizon in the upper third, road leading to a light source, car in the lower-centre third, foreground framing element on at least one side.
- Shoot into the sun for the silhouette-and-rim-light frame. Shoot with the sun behind for the paint-and-detail frame. You need both.
- Capture at 4K with a temporary higher-quality tier, then downscale. Downscaled supersampling looks dramatically better than native and costs nothing at capture time.

---

## 10. Manual steps this specification implies

Add these to `docs/MANUAL_STEPS.md` — Claude Code cannot do any of them:

- Model or heavily modify the three vehicles in Blender (original designs)
- Author the car paint, glass, and tail light materials
- Sculpt the landscape and place canyon rock assemblies
- Place the road spline and author its banking
- Author the asphalt master material and crack decal library
- Configure the lighting rig and grade the post-process volume
- Author the LUT
- Record and design the engine audio layers
- Tune every curve Claude generates as a stub

That list is roughly six of your twelve weeks. Plan accordingly.
