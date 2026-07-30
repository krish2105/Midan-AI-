# MIDAN — ART DIRECTION & RENDERING SPECIFICATION
**Attach alongside `MIDAN_UE5_RACING_CLAUDE_CODE_MASTER_PROMPT.md`**
Version 2.1 · Target: 1080p / 60fps primary, 1440p stretch

> **v2.1** — §8.1 gains an assist/off-track flag row, specified in §8.5. Approved after
> Phase 3 surfaced the states. No other section changed from v2.0.

> **v2.0 — art direction pivot.** v1.0 specified a golden-hour desert canyon with a single
> locked lighting condition. v2.0 replaces it with a **wet neon city circuit at night**,
> with **three static time-of-day presets** (Day, Night, Deep Night). §2, §3 and §7 are
> rewritten; §0, §4, §5, §6, §8, §9 carry forward substantially unchanged.
>
> **What this costs**, stated plainly so it is not discovered later:
> - Three lighting conditions triple lighting authoring and roughly triple PSO cache
>   coverage. The master prompt's §7.3 PSO workflow now needs three passes.
> - Wet road reflections, neon emissive spill and rain particles all add GPU cost that the
>   v1.0 budget did not carry. §7.1 is re-allocated accordingly.
> - A day/night *cycle* remains a **non-goal**. These are three discrete presets selected
>   before a race, not an animated sun. That distinction is what keeps the exposure clamp
>   in §6 workable and the PSO surface finite.
>
> The reference frames that prompted this pivot contain badged production vehicles and, in
> one case, a competitor's shipped-product screenshot with real sponsor trade dress. **None
> of that transfers.** What transfers is design language: wet asphalt with pooled specular,
> neon emissive spill onto the road, tight urban canyon, rain, motion-blurred speed. See §0.

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

**Night is the hero condition.** Author it first, tune it to finished, and treat Day as the
supporting variant. The reason is not aesthetic preference: at night the *scene lights
itself*. Neon signage, shopfronts and headlights become the key light sources, which means
emissive materials and Lumen do the work that a directional light does in daylight. Wet
asphalt then reflects all of it, and a modest asset budget reads as expensive because most
of what the player sees is light rather than geometry.

That is the same principle behind v1.0's golden hour — hide geometry deficiency behind
dramatic light — applied to a different hour.

### 2.1 The three presets

Three discrete lighting setups. **Not an animated cycle.** Each is a separate set of light
actor values plus a post-process preset, selected before the race starts.

| | **Deep Night** (hero) | **Night** | **Day** |
|---|---|---|---|
| **Read** | Rain-soaked, neon-dominated, near-black sky | Neon-dominated, drier, some ambient city glow | Overcast wet, flat sky, neon still visible |
| Directional light | **Off**, or moon at 0.02–0.05 lux | Moon 0.05–0.1 lux, 7000–8500K | Sun 8–15 lux, pitch −25° to −40°, 6500K |
| Sky Light intensity | 0.05–0.15, deep blue-violet | 0.15–0.3, blue | 1.0–2.0, neutral overcast |
| Dominant key source | **Emissive neon + headlights** | Emissive neon | Sky (overcast dome) |
| Fog density | Highest — neon bloom through haze | Medium | Low, cool grey |
| Wetness | Full — standing water, active rain | Damp — pooled specular, no rain | Damp |
| Exposure target | Lowest; let neon clip slightly | Mid | Highest |

Author in that column order: **Deep Night, then Night, then Day.** Day is the one most
likely to be cut if the schedule tightens, and building it last means cutting it costs
nothing already spent.

### 2.2 Neon as the lighting system

This is the single most important section in v2.0. At night, the signage *is* the lighting
rig.

- **Emissive materials on Lumen-visible geometry.** Sign faces, shopfront panels, tube
  lettering. Lumen picks up emissive as an indirect source, which is what spills coloured
  light onto the road and the car. This is why software-traced Lumen is worth its cost here
  rather than being a compromise.
- **A small number of saturated hues, deliberately chosen.** Cyan, magenta, warm amber, and
  one acid green as an accent. Twenty competing colours read as noise; four read as art
  direction. The reference frames work because each car sits against a *contrasting*
  dominant hue.
- **Rect lights for the hero signs only.** Emissive alone will not throw a crisp enough
  pool onto the road for the two or three signature corners. Add a Rect Light matched to
  the sign's colour and shape at those locations, and nowhere else — each one is a shadow
  caster and a real cost.
- **Colour separation left-to-right.** Where the road runs between two building faces, put
  opposing hues on either side (cyan left, magenta right). The car picks up both, gets
  automatic rim separation from the background, and the wet road doubles the effect.
- **No sign is a real brand.** Original signage only, original lettering, invented
  wordmarks. §0 applies to storefronts exactly as it applies to cars.

### 2.3 Wet road — where the look actually comes from

The wetness sells the neon. Dry asphalt at night is a black void; wet asphalt is a mirror
that doubles every light source in the scene.

- **Roughness is the whole trick.** A wet road is a low-roughness, high-specular surface
  with *variation* — pooled water at near-zero roughness, damp patches mid, dry patches
  under overhangs high. Uniform low roughness reads as polished plastic, not water.
- **Puddles as a mask, not as meshes.** Drive roughness and a subtle normal flattening from
  a large-scale mask along the spline. Standing water is where the road is lowest, so the
  mask should correlate with the road's banking.
- **Reflections come from Lumen**, not from planar reflections. Planar reflection actors
  cost a full scene re-render each and there is no budget line for that.
- **A faint normal-map ripple** on standing water, animated slowly. Barely perceptible, and
  it is what stops puddles reading as decals.
- **Rain is two systems**, and only one of them is particles: a Niagara rain volume for the
  falling streaks, plus a screen-space droplet/wiper treatment in post. The post layer is
  cheap and does more for the sensation of rain than the particles do.

### 2.4 Colour temperature contrast (the professional touch)

The v1.0 principle survives the pivot intact — only the sources change. Opposition is what
creates depth:

| | Warm side | Cool side |
|---|---|---|
| Deep Night | Amber sodium signage, headlights, brake lights | Cyan/magenta neon, blue-violet sky light, wet road reflection |
| Day | Nothing much — overcast is neutral | Cool grey sky dome |

If a night scene looks flat, the cause is almost always that **everything has drifted to the
same hue**. Push one side of the street warmer before touching anything else. And if a
shadow has gone neutral grey, raise sky light saturation — a night shadow should be
blue-violet or it reads as underexposed rather than dark.

Day is the harder preset to make interesting, precisely because it has no opposition. Lean
on wet-road specular and keep the neon *on* even in daylight — a lit sign against an
overcast sky is a real and appealing look, and it means the neon authoring is not wasted.

### 2.5 Time-of-day is three presets, not a cycle

**No animated sun. No dynamic transition.** A cycle would multiply the exposure-clamp
problem in §6 by every intermediate state, make the Lumen and VSM worst cases unbounded,
and expand PSO coverage without limit. It is also an explicit master-prompt non-goal.

Three presets is already three times the lighting authoring and roughly three times the
PSO cache work of v1.0. That is the accepted cost of this pivot. Anything beyond it is not.

**Consequence for §7.3 of the master prompt:** the scripted PSO collection playthrough now
runs three times, once per preset, and the bundled cache is the union. Skipping a preset
means shipping a build that hitches the first time a player selects it.

---

## 3. Environment bill of materials

Build the track as a kit, not as unique geometry. **This is more true for a city than it was
for a canyon** — urban geometry is inherently modular, and a city built as a kit is
convincing in a way that a canyon built as a kit is not.

The city also has a genuine advantage over the canyon at equal effort: **it occludes
itself.** Buildings block sightlines, which caps how much of the scene is ever visible at
once. An open canyon draws everything to the horizon. Expect the draw-call and Lumen
picture to be *better* here than v1.0 assumed, and the shadow picture to be much better —
see §7.

### 3.1 Buildings and the urban canyon

- **Modular facade kit** — a small set of storey-height panels (glass curtain wall, brick,
  concrete, shuttered shopfront) that tile vertically and horizontally. Nanite. Six to eight
  panel types assemble into an entire street.
- **Greeble on the upper storeys only.** Air-conditioning units, pipework, fire escapes,
  roof furniture. Nobody at 200 km/h looks above the third floor, so the detail budget
  belongs at street level where the camera actually is.
- **Ground floor gets the attention.** Shopfronts, awnings, doorways, railings, bollards,
  street-level clutter. This is the band the camera sees for the whole race.
- **Building height framing the road is the composition tool** the canyon walls used to be.
  Tall and close for a tunnel-like sprint section; lower and set back where the track opens
  into a plaza.
- **Interiors are a cheat.** Do not model them. A lit interior card behind the glass —
  emissive texture, parallax offset — reads correctly at speed and costs nothing.

### 3.2 Signage — the largest single art investment in v2.0

Signage is doing the lighting job the sun used to do (§2.2), which makes it a lighting
deliverable, not decoration.

- **A library of 15–25 sign meshes**: wall-mounted boxes, projecting blades, vertical tube
  lettering, large screen panels, scaffold-mounted billboards.
- **One master emissive material** with instance parameters for hue, intensity, flicker rate
  and animated-scroll speed. Twenty-five signs from one material keeps the permutation count
  inside the §7 master-material limit.
- **Animate a few, not most.** Two or three flickering or scrolling signs read as a living
  city. Twenty read as a broken renderer.
- **Large screen panels** on two or three hero buildings, playing an original looping
  texture. This is the Times-Square read from the reference, and it is one material and one
  mesh.
- **Every wordmark is invented.** No real brand, logo, or recognisable typography treatment.
  §0 governs signage exactly as it governs cars — a real storefront logo is the same
  category of liability as a real badge.

### 3.3 Road

Mostly carried from v1.0, with wetness promoted from a fake to a real system (§2.3).

- **Spline mesh road** from `AMidanTrackSpline`.
- Base asphalt: scanned road material, 2–4 m tiling, with a large-scale macro variation mask
  breaking up repetition.
- **Crack and patch decals** — 8–12 scattered along the spline. Nearly free, and they read
  strongly at night because wet cracks catch light differently from wet asphalt.
- **Road markings as decals**, never baked into the base material — so the line pattern is
  authored independently of road geometry.
- **Wetness mask** driving roughness variation and puddle placement. Per §2.3 this is the
  system the whole look rests on, not a texture detail.
- **Manhole covers, drain grates, tram or service rails, painted crossings** as decals and
  small meshes. Urban road furniture is what distinguishes a city street from a grey ribbon,
  and it is all decals.
- **Reflective road markings** — wet white paint under neon is one of the strongest images
  available here, and it is free once the wetness mask exists.

### 3.4 Furniture and scatter

- **Barriers**: concrete blocks, water-filled barriers, and steel armco, instanced along a
  spline. A street circuit is *defined* by its barriers, and they double as the light-blocking
  geometry that makes neon pool.
- **Street furniture**: lamp posts, traffic lights, bus shelters, benches, bins, phone boxes,
  news stands. One instanced set placed along splines.
- **Overhead structure**: gantries, pedestrian bridges, hanging cables, banner lines across
  the street. These are the strongest framing devices in the whole kit — a gantry silhouetted
  against a lit sign is a screenshot on its own.
- **PCG** for street-level scatter — litter, leaves, small debris against kerbs — with a
  distance-from-spline exclusion mask so nothing spawns on the racing surface. PCG's role
  changes from desert scrub to urban clutter; the exclusion-mask technique is identical.
- **Parked vehicles** as static background meshes. Original designs only, and they can be
  much lower detail than the hero cars. A street with no parked cars reads as a set.

### 3.5 Composition rule, restated for the city

v1.0's rule was *the road curves out of frame toward the light*. The city version:

**Design at least three points on the circuit where a corner exit frames a dominant lit
structure** — a hero sign, a screen-panel building, a lit gantry — with the road's wet
surface reflecting it back toward the camera. Those are the screenshot and trailer moments,
and they should be placed deliberately at the layout stage, not discovered afterwards.

Two further city-specific rules:

- **Vary the width.** A street circuit that is one width throughout feels like a corridor.
  Pinch it to barely two cars wide somewhere, and open it into a plaza somewhere else.
- **Put one elevation change in.** An underpass or a bridge crossing gives you a section
  with completely different lighting — darkness with hard-edged pools of light — for almost
  no extra art. It is the cheapest variety in the whole kit.

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

### 7.1 Revised budget — 1080p/60 primary · v2.0 re-allocation

The pivot to night is not uniformly more expensive. It **removes v1.0's single largest
risk** — a −3° to −6° directional light casting shadows across open terrain — and spends
the saving on reflections, which is where the neon look actually lives.

| Line | v1.0 (canyon, golden hour) | **v2.0 (neon city)** | Δ |
|---|---|---|---|
| Total frame | 16.6 | **16.6** | — |
| GPU total | ≤ 15.0 | **≤ 15.0** | — |
| — Base pass (Nanite) | ≤ 3.0 | **≤ 3.0** | — |
| — Lumen GI + reflections | ≤ 3.5 | **≤ 4.0** | +0.5 |
| — Virtual Shadow Maps | ≤ 2.5 | **≤ 1.5** | **−1.0** |
| — Volumetric fog (clouds cut) | ≤ 1.5 | **≤ 1.0** | −0.5 |
| — Local lights + emissive | *(absent)* | **≤ 0.75** | +0.75 |
| — Post + TSR | ≤ 2.5 | **≤ 2.5** | — |
| — Translucency / particles / rain | ≤ 1.0 | **≤ 1.25** | +0.25 |

Sub-lines sum to **14.0 ms**, preserving the **1.0 ms unallocated contingency** v1.0 held.
The re-allocation is net zero by construction — the VSM saving funds everything else.

**Why each line moved:**

- **VSM −1.0.** Deep Night has the directional light *off* and Night has it at moonlight
  intensity. The catastrophic case v1.0 warned about does not exist in the hero preset. Local
  shadow casters from a handful of rect lights are far cheaper than a full directional
  cascade across open terrain.
- **Lumen +0.5.** Now the hero cost. Wet-road reflections plus emissive signage as an
  indirect source is precisely what makes the look, so it gets the budget. The city's
  self-occlusion partly offsets this — a street has far shorter sightlines than an open
  canyon, so trace distances are shorter.
- **Volumetric −0.5, clouds cut entirely.** At night there is no cloud detail to see. Fog
  stays and stays important — it is what makes neon bloom through haze — but 1.0 ms is
  enough for fog alone.
- **Local lights + emissive, new at 0.75.** Rect lights on the two or three hero signs, plus
  the Lumen scene-update cost of many emissive surfaces. This was zero in v1.0 because the
  canyon had one light source.
- **Translucency +0.25.** Rain particles and wheel spray. Modest because the screen-space
  droplet layer in post does most of the rain sensation, and it is cheaper.

**Hold the budget at the worst preset, per line.** Day is now the VSM worst case because it
is the only preset with a real sun; Deep Night is the worst case for Lumen, translucency and
local lights. A budget that passes on Deep Night and fails on Day has not passed.

### 7.2 The four costs that will bite

Reordered for v2.0. The old number-one risk is gone; these are the new ones.

1. **Lumen reflections on wet asphalt.** A low-roughness surface covering the entire play
   area is the worst possible input for a reflection system. Keep **software tracing**, clamp
   max trace distance to the street width rather than leaving it at default, and if it will
   not fit, reduce reflection *quality* before reducing wetness — a lower-quality reflection
   on a wet road still reads as wet, but a dry road does not read at all.
2. **Emissive count and Lumen scene update.** Every emissive surface is a potential indirect
   source. Many small signs cost more than a few large ones for the same visual result.
   Consolidate: one large sign beats six small ones, both artistically and in the profile.
3. **Shadow-casting local lights.** Each rect light that casts is a real cost. §2.2 limits
   these to hero signs deliberately. If the count creeps past four or five, this line will
   blow before any other.
4. **Rain particle overdraw.** Rain is translucent geometry filling the screen — the classic
   overdraw case. Measure it alone. Prefer fewer, larger, better-textured streak particles
   over many thin ones, and lean on the post-process droplet layer.

**Plus a schedule cost, not a frame cost:** three presets means the master prompt's §7.3 PSO
collection playthrough runs three times and the shipped cache is the union of all three.
Skipping one ships a build that hitches the first time a player selects it.

### 7.3 What to cut first if you are over budget

Revised order for v2.0:

1. Volumetric clouds — already effectively cut at night; remove entirely
2. **Rain particle density** — the post droplet layer carries the effect
3. Lumen reflection *quality* (never wetness itself — see §7.2.1)
4. **Number of shadow-casting rect lights**
5. VSM resolution
6. Screen percentage — TSR absorbs a surprising amount
7. Street-level PCG scatter density

**Never cut the film grain, the exposure clamp, or the motion blur.** Those are feel, not
fidelity, and cutting them costs more than the frame time returns. Film grain matters *more*
in v2.0 than v1.0: night scenes have large smooth dark gradients, which band worse than a
bright sky does.

**Also never cut the wet-road roughness variation.** It is a material, not a render pass —
it costs essentially nothing and it is the entire look. There is no version of this art
direction with a uniform-roughness road.

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
| **Assist / off-track flags** | **Top-left** | **TC, ABS, OFF TRACK. From `UVehicleAssistComponent::GetActiveInterventions()` and `UVehicleSurfaceSensorComponent::GetOffTrackWheelCount()`. Approved v2.1 — see §8.5.** |

### 8.5 Assist and off-track flags — approved v2.1

Added after Phase 3, which surfaced all three states. Not in the original reference frame,
and it is the one HUD element this project has that the reference did not.

| Flag | Lit when | Why the player needs it |
|---|---|---|
| **TC** | `EMidanAssistFlags::TractionControl` is intervening | Traction control cutting throttle feels identical to the engine going flat. Without an indicator the player attributes an assist to a broken car. |
| **ABS** | `EMidanAssistFlags::ABS` is intervening | Brake release under lock reads as brake failure. Same reasoning. |
| **OFF TRACK** | `GetOffTrackWheelCount() > 0` | Lap invalidation has a grace period (§3.2). Without a live indicator the player learns their lap was voided several seconds after the cause, which reads as arbitrary. |

Rules that follow:

- **Momentary, not sticky.** These reflect live state. A latched indicator would tell the
  player that TC fired at some point, which is not actionable.
- **Amber for TC and ABS, red for OFF TRACK.** Off-track has a rules consequence; the
  assists do not.
- **Text label, not an icon.** Three-letter labels are unambiguous at speed; invented
  iconography needs learning, and this slice gives the player 90 seconds.
- **Fades with the minimal-HUD toggle** (§8.4) — informative, not critical.
- **Colour is not the only channel.** OFF TRACK is distinguished from the assists by its
  label and its position in the row, so the §8.4 colourblind rule holds without a fourth
  visual language.

Stability control and steering assist deliberately get **no flag**. Both are off by default
on the GT, both intervene continuously rather than in discrete events, and an indicator that
is lit most of the time is wallpaper.

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
