---
name: game-feel-engineering
description: Use when implementing or tuning the systems that make control feel good rather than merely work — camera behaviour and FOV response, input shaping and buffering, screen shake, audio parameter mapping, haptics, particle and decal feedback, hit stop, and the curves behind all of them. Trigger for "the camera feels", "it feels floaty/sluggish/disconnected", "add screen shake", "speed sensation", "input feels laggy", "make it feel punchy", "camera lag", "engine audio blending". Do NOT trigger for the underlying simulation or physics parameters themselves, or for visual art direction.
---

# Game Feel Engineering

Feel is roughly 40% of how a game is perceived and roughly 10% of the code. It is also the
part most often left as an afterthought, which is why so many technically correct games are
unpleasant to control.

**The core discipline: every feel value is a curve, not a constant.** A constant produces a
system that is right at one speed or one intensity and wrong everywhere else. Curves are
also what makes feel tunable by a human without a recompile — put them in Data Assets, and
put nothing else there.

---

## 1. The feel loop

Feel is a loop, not a feature: **input → simulation → feedback**. A weakness anywhere reads
as "the controls are bad", even when the culprit is the camera or the audio.

| Stage | Failure mode | Reads to the player as |
|---|---|---|
| Input | Raw, unshaped, unbuffered | Twitchy, or unresponsive |
| Simulation | Correct but uncommunicative | Floaty, disconnected |
| Feedback | Late, flat, or absent | Weightless, unsatisfying |

Diagnose in that order. Players almost always misattribute feedback problems to input.

---

## 2. Input shaping

**Never map raw input directly to an output magnitude.** This is the single most common
cause of a control scheme that feels wrong.

- **Speed-sensitive response.** The output that feels right at low speed is unusable at high
  speed. Scale the maximum output by a curve over speed.
- **Separate rise and fall rates.** Fast attack with slower release feels responsive but
  stable. The reverse feels sluggish and nervous. One symmetric rate is a compromise that
  satisfies neither.
- **Digital input needs its own curve.** Keyboard or button input ramping through the same
  curve as an analogue stick is a distinct failure. Ramp in over roughly 100–200 ms, with
  its own shape — not a scaled copy of the analogue curve.
- **Deadzone at the low end, saturation at the high end.** Both should be tunable; stick
  hardware varies more than people expect.

### Forgiveness features that read as responsiveness

These cost little and are consistently mistaken by players for "tighter controls":

- **Input buffering** — accept an action pressed slightly too early and fire it when valid
- **Coyote time** — allow the action for a short window after the condition lapses
- **Snapping and assist** — bias toward the intended target within a small envelope

Every one of these is a lie told in the player's favour. That is the job.

---

## 3. Camera — the largest feel lever available

The camera does more for the sensation of speed and weight than the simulation does.

| Behaviour | Implementation | Why it matters |
|---|---|---|
| **FOV response** | `FOV = Base + Curve.Eval(speed01)` | The single biggest speed-sensation lever. A tight base widening under speed does more than any particle effect. |
| **Positional lag** | Spring-arm lag, separate location and rotation rates, curve-driven by speed | Lag is what makes the subject feel like it has mass the camera must chase |
| **Look-ahead** | Yaw or position offset from steering/aim input, damped | The camera anticipates rather than follows — reads as competence |
| **Slip / drift yaw** | Additional yaw from the subject's slip angle | Makes a slide legible; without it a drift looks like a bug |
| **Vertical damping** | Suppress high-frequency vertical chatter, preserve large impacts | Suspension or footstep chatter reaching the camera is nauseating; a real impact must still land |
| **Impact shake** | Amplitude curve-mapped from impulse magnitude | Unscaled shake makes every impact feel identical, so none feel big |
| **Surface / state rumble** | Low-amplitude noise driven by surface roughness or state | Continuous texture the player reads without noticing |

Rules that apply to all of them:

- **Use frame-rate-independent damping.** `Lerp(current, target, rate * dt)` is wrong at
  varying frame rates — the effective smoothing changes with frame time. Use an exponential
  form: `target + (current - target) * exp(-rate * dt)`.
- **Clamp everything.** Unclamped FOV, shake, or offset produces a spectacular failure
  exactly once, in front of someone important.
- **Multiple camera modes belong behind one interface**, so a mode is a data-driven variant
  rather than a branch in the camera code.
- **Motion blur is a feel system, not only a rendering one.** Object blur conveys speed.
  Expose the intensity — a meaningful minority of players find it unpleasant, and some need
  it off for motion sensitivity.

---

## 4. Audio as feedback, not decoration

Audio is where feel is most often left flat, because a single sample crossfaded on one axis
technically works.

- **Blend on at least two axes.** For an engine: RPM on one axis, load or throttle on the
  other, with separate on-load and off-load sample sets. Single-axis RPM blending produces a
  whine; two-axis blending produces an engine. The same principle applies to any continuous
  sound — footsteps on surface × speed, weapon handling on state × urgency.
- **Layer, do not replace.** Crossfading between two samples loses the sense of a
  continuous source. Overlapping layers with independent gain curves keeps it.
- **Drive it from simulation state, not from animation events**, wherever the state exists.
  Slip-driven tyre scrub responds to the actual physics; an animation-triggered skid sound
  does not.
- **Keep sound design in the authoring tool and parameter mapping in code.** The mapping is
  engineering; the sample content is taste. Mixing them makes both harder to change.
- **Vary everything.** Fixed pitch and fixed volume on a repeated sound is fatiguing within
  a minute. Small randomisation per instance, plus a minimum retrigger interval.

---

## 5. Haptics

Underused, and cheap. The controller is a second output channel most projects leave silent.

- Continuous low-amplitude effects for state — idle, surface texture, strain
- Sharp transient effects for events — impacts, shifts, hits
- **Scale by magnitude**, from the same curve family as the visual and audio response, so
  all three channels agree
- Provide an intensity slider and an off switch. Accessibility, and some players simply
  dislike it.

---

## 6. Visual feedback

- **Scale with intensity.** A particle rate, decal opacity, or post-process intensity driven
  by a curve over the relevant magnitude beats an on/off trigger every time.
- **Pool everything that accumulates.** Decals, particle systems, and debris deposited over
  a session are a slow leak that only appears on the fourth lap or the tenth minute. Fixed
  budget, oldest recycled.
- **Hit stop / time dilation** — a few frames of pause on a significant impact is the
  cheapest possible way to give an event weight. Milliseconds, not tenths of a second.
- **Restraint on full-screen effects.** Chromatic aberration, vignette, and bloom at
  perceptible strength read as amateur; at barely-perceptible strength scaled by state they
  read as photographic. If a reviewer can name the effect, it is too strong.

---

## 7. Tuning method

Feel cannot be tuned by reading numbers. It is tuned by feel, which means the loop from
change to perception must be short.

1. **Make it hot-reloadable.** Every feel value in a Data Asset, applied through a hot-reload
   delegate in editor. A recompile per iteration means an order of magnitude fewer
   iterations, and it will show.
2. **Change one thing at a time.** Two simultaneous changes teach you nothing about either.
3. **Overshoot deliberately, then come back.** Push a value until it is obviously wrong in
   both directions to find the usable range, then bisect. Starting from a plausible value
   and nudging finds a local optimum and stays there.
4. **Take breaks.** Sensitivity to feel degrades within about twenty minutes of continuous
   tuning, and the version you converge on while numb will be over-tuned.
5. **Test at multiple frame rates.** Feel bugs hide behind frame-rate dependence, and this
   is where the exponential-damping rule earns itself.
6. **Keep a reference.** Save the last version you were happy with. Regression in feel is
   invisible without an A/B.

**Budget real time for this.** Feel tuning is not an afternoon of number-twiddling; studios
employ specialists for it. Code can deliver every system and every curve, correctly wired,
and the result will still feel wrong until a human sits down and tunes it.

---

## 8. Checklist

- [ ] Every feel value is a curve in a Data Asset, not a constant in code
- [ ] Hot-reload path exists so iteration does not require a recompile
- [ ] Raw input never maps directly to output magnitude; speed-sensitive curve present
- [ ] Separate rise and fall rates; separate curve for digital input
- [ ] All damping frame-rate independent (exponential, not `Lerp` on raw dt)
- [ ] FOV response curve present and clamped
- [ ] Camera lag, look-ahead, slip yaw, vertical damping, impact shake all magnitude-scaled
- [ ] Camera modes behind one interface
- [ ] Audio blends on at least two axes, layered rather than crossfaded
- [ ] Audio driven by simulation state where available, with per-instance variation
- [ ] Haptics scaled from the same curves as visual and audio, with an off switch
- [ ] Accumulating visual feedback pooled with a fixed budget
- [ ] Full-screen post effects subtle and state-scaled
- [ ] Intensity sliders exposed for motion blur, shake, and haptics
- [ ] Verified at more than one frame rate
