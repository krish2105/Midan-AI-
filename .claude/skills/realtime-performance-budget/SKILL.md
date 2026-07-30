---
name: realtime-performance-budget
description: Use when setting, holding, or reporting a real-time frame budget — allocating milliseconds across GPU passes and CPU threads, deciding which lines block a build versus which are tuned, profiling methodology, deterministic replay for comparable runs, or writing a performance table into a README or phase gate. Trigger for "frame budget", "60 fps target", "is this fast enough", "profile this", "stat unit", "GPU time", "draw calls", "performance regression", "what should I cut". Do NOT trigger for algorithmic big-O work or backend latency with no frame deadline.
---

# Real-Time Performance Budget

A frame budget is not a wish. It is a contract with a fixed total that you allocate, hold,
and report against. Most projects fail it in one of three ways: never writing it down,
writing it down and never measuring, or measuring in a way that makes the numbers
meaningless.

---

## 1. Allocate from the deadline backwards

Start from the frame deadline and divide. 60 fps is **16.6 ms**; 30 fps is 33.3 ms;
120 fps is 8.3 ms. Every subsystem gets a number, and the numbers sum to less than the
total.

**Leave unallocated headroom.** If your sub-lines sum exactly to the total, you have
budgeted for the average frame and will miss on every worst case. Reserve 5–10% for
variance — the frame where everything is on screen at once. That headroom is contingency,
not spare capacity: spending it on a new feature requires a written trade against an
existing line.

Budget the **CPU threads separately from the GPU**, because they fail differently and are
fixed differently. A GPU-bound frame is solved by cutting fidelity; a game-thread-bound
frame is solved by cutting work or moving it off-thread, and no amount of resolution
reduction helps.

A resolution change relieves GPU cost only. Draw calls, game-thread work, and animation
cost are resolution-independent — do not loosen those lines when you drop resolution.

---

## 2. Three kinds of line, treated differently

This is the distinction that makes a budget survive contact with a deadline.

### Hard gates — binary, block the build

Target is a threshold, not a direction. There is no "we improved it to 17.1 ms".

- Total frame time at **p95**
- Peak draw calls
- Zero allocations in the hot path
- Zero blocking I/O on the frame thread
- Editor-only or debug-only code absent from the shipping binary

Wire these into CI. Do not negotiate with a gate.

### Tuning metrics — report the number and the trend

Individual GPU passes and thread times. **A single sub-line over budget is a conversation,
not a failure, provided the total still holds.** If shadows come in 0.5 ms over and
post-processing 0.5 ms under, the frame is fine and the report should say so rather than
raising a false alarm. A budget that cries wolf on every sub-line gets ignored entirely.

### Operating metrics — whether it can actually ship

Package size, cold launch time, hitch count, build time. Excellent frame time with a
40-second launch and visible shader hitching is not a shippable product. These are
routinely omitted from performance reports and are often what a reviewer notices first.

---

## 3. p95, not mean

A mean of 15 ms with a p95 of 24 ms is a game that stutters, and the mean actively hides
it. Players perceive the worst frames, not the average frame — one 30 ms frame per second
is more noticeable than a uniformly slower frame time.

Report p95 for the gate. Report max, too, when hitching is the concern. Report the
**run-to-run max delta** so you know whether the measurement is stable enough to act on:
unbounded variance between identical runs means the number is noise, and tuning against
noise wastes days.

---

## 4. Measurement provenance — five fields, or it is not a number

Every reported figure states:

1. **Build configuration.** Profile a shipping-like configuration with stats enabled, never
   a development build. Development-build numbers say nothing about the shipped product,
   and this is the most common way a performance claim becomes worthless.
2. **Resolution and any upscaling factor.** "1080p" is ambiguous when the upscaler renders
   at 67%.
3. **Machine, GPU, OS, driver.** Different graphics backends are not comparable.
4. **Quality tier.** Which scalability level was active.
5. **Run count and statistic.** Minimum 3 runs; state p95 and max delta.

Then:

- **Report failing lines by name.** A table with no failures listed is a table nobody
  believes.
- **Never report an aggregate that hides a segment failure.** "GPU total 14.2 ms" while one
  pass is 60% over its line is a misleading pass. Break it out.
- **State the limitation you already know about**, before a reviewer finds it. "Shadows are
  1.1 ms over budget under a low sun; the fix is a resolution bias clamp, roughly an hour"
  reads as competence. Omitting the row reads as evasion.
- **Never state a number you have not measured.** Not in a table, not in a README, not in a
  commit message. An estimate labelled as an estimate is fine; an estimate presented as a
  measurement is not.

---

## 5. Deterministic replay makes runs comparable

Without it, every performance comparison is between two different play sessions and any
difference could be the input rather than the build.

Record a representative run's **inputs**, then replay them deterministically. Now a
regression between builds is attributable. This is the single highest-leverage piece of
performance infrastructure, and almost nobody builds it — which is exactly why having one
is worth stating out loud.

Replay inputs, not transforms. Input replay exercises the real simulation; transform replay
measures a camera flythrough and misses everything the simulation costs.

Automate the parse: a script that reads the trace and emits the budget table with pass/fail
per line means the numbers get generated on every build rather than when someone
remembers.

---

## 6. Decide the cut order in advance

When you are over budget, you are also under time pressure and about to make a bad decision.
Write the order down while calm.

Order the list by **cost saved per unit of perceived quality lost**. Cut the expensive,
barely-noticed thing first.

And name the things you will **never** cut. Typically these are perception and feel systems
that are cheap in milliseconds but expensive in how the product reads — grain that hides
banding, an exposure clamp that stops the image hunting, motion blur that conveys speed.
Cutting them returns almost no frame time and costs more than it saves. Making that
explicit prevents someone from "optimising" them out at 2 a.m.

---

## 7. Know your worst case, and check it deliberately

Every project has a frame that is the worst case by construction. Find it early:

- The lighting condition your art direction chose, at its most expensive angle
- The most objects that can be on screen at once
- The most particles the gameplay can legitimately spawn
- The streaming boundary crossed at maximum traversal speed

Profile that frame, not a quiet one. A budget held only in the easy case is not held.

Also measure expensive features **in isolation** before deciding whether they earn their
cost. "Volumetric clouds cost 2.1 ms" is actionable; "the frame is over budget" is not.

---

## 8. Streaming needs travel-time headroom, not distance

Streaming distance is a function of traversal speed. At 300 km/h an object covers 83 m per
second, so a streaming radius must exceed several seconds of travel — plus the actual load
time — or the player outruns the loader and sees pop-in.

Compute it from top speed rather than picking a distance that looks reasonable in the
editor at walking pace.

---

## 9. Shader compilation hitching is a separate budget

Frame time can be perfect while the build feels broken. Shader pipeline state object
compilation on first encounter produces multi-frame hitches, and it does not appear in the
editor because the editor already compiled everything.

The fix is a scripted playthrough covering every combination the player can reach —
every vehicle, every camera mode, every surface, every lighting state — collecting the
cache and bundling it. **Measure hitch count before and after and report both numbers.**
One number without the other proves nothing.

This is the most common reason an indie build feels worse than the editor did, and the fix
is an afternoon of scripting.

---

## 10. Reporting template

```
FRAME BUDGET REPORT
Config: <shipping-like + stats> · <res> @ <upscale%> · <machine/GPU/OS>
Tier: <quality tier> · <n> runs · reporting p95

HARD GATES
  frame_time_p95_ms          15.9   [<=16.6]   PASS
  draw_calls_peak            2840   [<=3000]   PASS
  hotpath_allocations           0   [==0]      PASS

TUNING
  base_pass_ms                2.8   [<=3.0]    PASS
  gi_reflections_ms           3.9   [<=3.5]    OVER  (+0.4)
  shadows_ms                  2.2   [<=2.5]    PASS
  post_upscale_ms             2.3   [<=2.5]    PASS
  gpu_total_ms               14.6   [<=15.0]   PASS
  game_thread_ms              5.4   [<=6.0]    PASS

OPERATING
  package_size_mb             ...   [<=N]      PASS
  cold_launch_seconds         ...   [<=N]      PASS
  hitch_count_pre_cache       ...   [baseline]
  hitch_count_post_cache      ...   [< pre]    PASS
  run_to_run_max_delta_ms     0.7   [<=1.0]    PASS

Failures: gi_reflections_ms over by 0.4ms. GPU total still within budget, so
not gating. Cause: trace distance unclamped in open areas. Fix: clamp to
scene bounds, ~1 hour.
Known limitations: <stated>
```

Note what that report does: the one line over budget is named, its consequence is assessed
against the total rather than in isolation, the cause is identified, and the fix is sized.
That is what makes a performance table credible.

---

## 11. Checklist

- [ ] Budget written down before the work, with unallocated headroom
- [ ] CPU threads budgeted separately from GPU
- [ ] Each line classified as hard gate, tuning metric, or operating metric
- [ ] Hard gates wired into CI
- [ ] p95 reported, not mean; run-to-run delta reported
- [ ] All five provenance fields on every number
- [ ] Deterministic input replay exists, so runs are comparable across builds
- [ ] Trace parsing automated into the budget table
- [ ] Cut order decided in advance, including what will never be cut
- [ ] Worst-case frame identified and profiled, not a quiet one
- [ ] Expensive features measured in isolation before being kept
- [ ] Streaming distance computed from top speed
- [ ] Shader-compilation hitching measured before and after cache bundling
- [ ] Failures named; no unmeasured number reported anywhere
