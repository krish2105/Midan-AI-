---
name: defensible-data-analysis
description: Use when analysing measured data and presenting conclusions someone will act on — telemetry, lap times, A/B comparisons, benchmark results, survey or log aggregates, or any chart going into a report or README. Covers denominators, matched-condition comparison, sample size, confounds, effect size versus significance, and chart honesty. Trigger for "analyse this data", "compare these runs", "is this improvement real", "make a chart of", "what does the telemetry say", "which is faster". Do NOT trigger for building the capture pipeline itself, or for exploratory plotting you will throw away.
---

# Defensible Data Analysis

The failure mode is not bad statistics. It is a plausible chart that answers a different
question than the one asked, presented with enough confidence that nobody checks. This
skill is the set of habits that make an analysis survive someone competent looking at it.

**The test to apply to your own output:** if a skeptical reader asked "compared to what, out
of how many, and under what conditions?", does the chart already answer them? If not, it is
not finished.

---

## 1. Every number carries its denominator

"94% faster" is not a claim. "94% of 17 laps" is a claim, and a weak one.

- State the sample count **next to the metric**, not in a caption or footnote.
- A percentage without its denominator is decoration. So is a mean without an n.
- If the n differs between compared groups, say so — unequal groups are fine, silently
  unequal groups are not.
- If you filtered rows out, report how many and why. "Excluded 6 of 34 laps: 4 invalidated
  for off-track, 2 incomplete" is honest. Dropping them silently changes the population
  you are describing.

---

## 2. Compare only within matched conditions

Most wrong conclusions come from comparing two things that differed in more ways than the
one you are studying.

Before comparing, enumerate what changed. For a lap-time comparison that means: same
vehicle, same track, same surface state, same assists, same difficulty, same build. If any
of those differ, you are measuring their sum, not your variable.

When you cannot match, the options in order of preference:

1. **Stratify** — compare within each condition and report per-condition results
2. **Control explicitly** — hold the confounds fixed even if it costs sample size
3. **Report the confound** — "the AI laps are on a later build; treat the delta as an upper
   bound"

Never do the fourth thing, which is to pool everything and hope.

**The nastiest confound is learning.** Anything a human does repeatedly improves over the
session. Comparing your first ten laps in car A against your last ten in car B measures
practice, not the cars. Randomise the order, or interleave.

---

## 3. Effect size before significance

"Statistically significant" answers whether an effect exists. It does not answer whether
the effect matters, and with enough samples trivial effects become significant.

Lead with the magnitude and its uncertainty: "0.4 s per lap, 95% CI [0.1, 0.7]" tells the
reader everything. "p < 0.05" tells them almost nothing.

For small samples — which most hand-collected data is — prefer showing the distribution
over a test. A dot plot of 12 laps per condition is more informative and more honest than a
t-test on 12 laps, because the reader can see the overlap themselves.

And distinguish **noise from signal by measuring the noise first.** Run the identical
condition twice and report the spread. If run-to-run variance is 0.5 s and your measured
effect is 0.3 s, you have not found an effect — you have found your own noise floor. This
one check invalidates more analyses than any other.

---

## 4. Never report an aggregate that hides a segment

A mean is a summary, and summaries lose exactly the information that is often the point.

- Report by segment whenever segments exist — per corner, per sector, per surface, per
  vehicle. "AI is 2 s slower overall" is far less useful than "AI loses 1.8 s of that in
  the three slow corners and is faster on the straights", which tells you what to fix.
- Show the distribution, not only the centre. Bimodal data has a meaningless mean.
- Report p95 or max alongside the mean whenever the tail is what the user experiences —
  frame time, latency, load time. A good mean with a bad tail is a bad experience.

---

## 5. Ground truth must be independent of the thing being measured

If the system under study generated its own reference values, the analysis is circular and
proves nothing.

- A model's own confidence is not a measure of its accuracy.
- A build's own self-reported timing is weaker evidence than an external profiler.
- "The AI's target speed" is not evidence the AI's target speed is achievable — a human
  lap is.

Where an independent reference is genuinely unavailable, say so explicitly and describe
what the comparison can and cannot establish.

---

## 6. Chart honesty

The chart is the argument. Most misleading charts are not fabricated; they are truthful
data drawn in a way that suggests a stronger conclusion than the data supports.

| Rule | Why |
|---|---|
| Zero baseline for bar charts | Truncated axes turn a 2% difference into a visual doubling |
| Truncated axes are fine for **line** charts of a varying quantity, but label the range clearly | A speed trace does not need to start at zero; a bar chart does |
| Same axis range when charts are compared side by side | Different scales make an eyeball comparison wrong |
| Plot raw points over, or instead of, bars when n is small | Twelve points shown beats twelve points averaged |
| Show uncertainty — error bars, CI bands, or the raw spread | A point estimate with no uncertainty reads as precision you do not have |
| Label units and direction | "Delta" is ambiguous; "Delta vs personal best (s), negative = faster" is not |
| Never use colour as the only encoding | Fails for colourblind readers and in greyscale print |
| One question per chart | A chart answering three questions answers none clearly |

State the sample count **in the chart**, not just the prose around it. Charts get screenshotted
away from their captions.

---

## 7. Write the conclusion the data supports, not the one you expected

- Name the limitation you already know about before a reader finds it. It reads as
  competence; omitting it reads as evasion, and it will be found.
- Publish the result that went the wrong way. An analysis with no negative findings is one
  nobody believes.
- Distinguish measured from inferred from assumed, in the text. Three different epistemic
  statuses, and readers cannot tell them apart unless you mark them.
- Prefer "consistent with" to "proves". Observational data rarely proves anything.
- If the honest answer is "the sample is too small to tell", that is a finding. Report it
  and say what sample size would settle it.

---

## 8. Reproducibility

An analysis nobody can re-run is an opinion with a chart attached.

- Script it. A notebook or script that goes from raw capture to final figure, with no
  manual steps in between.
- Version the input data, or record its hash and provenance.
- Record the tool versions and the date.
- Re-running the script on the same input must produce the identical figure. If it does not,
  find the nondeterminism before trusting any of it.

---

## 9. Checklist

- [ ] Every metric shows its n, in the chart itself
- [ ] Filtered or excluded rows counted and explained
- [ ] Compared groups matched on every condition except the variable of interest
- [ ] Confounds enumerated; learning/order effects controlled or randomised
- [ ] Noise floor measured by repeating an identical condition
- [ ] Effect size and uncertainty reported, not just significance
- [ ] Segment breakdown given wherever segments exist
- [ ] Tail statistic reported where the tail is what users feel
- [ ] Ground truth independent of the system under study
- [ ] Charts: honest axes, uncertainty shown, units and direction labelled, not colour-only
- [ ] Limitations and negative results stated explicitly
- [ ] Measured / inferred / assumed distinguished in the text
- [ ] Whole analysis reproducible from raw data by one script
