# Vehicle Roster — 3 cars, deliberately different personalities

Three cars that feel *the same* is a worse portfolio than one car that feels great. These
three are built to argue with each other.

Names are **original and fictional**. See §Naming for the scheme and the one verification
step still outstanding.

---

## 1. SAHM — Hypercar

**Class:** Hyper · `Vehicle.Class.Hyper` · **Drivetrain:** AWD · **Mass:** ~1400 kg · **Power:** ~800 hp

Personality: brutal straight-line speed, high downforce, planted above 150 km/h, nervous
below 80. Rewards commitment, punishes lift-off mid-corner.

- **Torque curve:** peaky, strong top end
- **Handling bias:** mild understeer under power, neutral off-throttle
- **Aero:** rear-biased downforce — this is what produces "planted at speed, nervous below
  80". Below `MinSpeedForDownforceKmh` there is no aero at all, so the mechanical balance
  shows through and the car feels like a different vehicle at low speed.

**Concept reference:** `docs/concept/concept_hyper.png`

---

## 2. RAQS — GT

**Class:** GT · `Vehicle.Class.GT` · **Drivetrain:** RWD · **Mass:** ~1550 kg · **Power:** ~500 hp

Personality: the drift car. Progressive oversteer, holdable slides, forgiving at the limit.
The one players will spend most time in.

- **Torque curve:** flat, wide
- **Handling bias:** rear friction below front. Oversteer character.
- **Tyre slip graph:** rounded peak with a *gentle* fall-off. This matters more than the
  absolute grip level — a gentle fall-off is what "forgiving at the limit" means
  mechanically, and a sharp peak would give a knife-edge car that snaps.
- **Assists:** stability control **off** by default. A stability system good enough to be
  invisible would also remove the controllable slides that are this car's entire reason to
  exist.

**Concept reference:** `docs/concept/concept_gt.png`

---

## 3. HAJAR — Rally

**Class:** Rally · `Vehicle.Class.Rally` · **Drivetrain:** AWD · **Mass:** ~1250 kg · **Power:** ~350 hp

Personality: slow on tarmac, unstoppable on gravel. High suspension travel, visible body
movement, deliberately high centre of mass.

- **Torque curve:** torquey low end
- **Handling bias:** surface-dependent. **The car that makes physical materials matter.**
- **Centre of mass:** deliberately high. The validator *warns* on this and the warning is
  correct to ignore — a high COM is a design dial here, not a bug. Quietly lowering it to
  stop a flip would remove exactly the character this car exists for.
- **Suspension:** long travel, under-damped relative to the other two, so the body moves and
  communicates constantly.

**Concept reference:** `docs/concept/concept_rally.png` — note the tail-lamp defect recorded
in `docs/ASSET_LICENCES.md`.

---

## Naming

**Scheme:** Arabic, matching the project name (**Midan**, ميدان — "the arena / the field").
One word each, chosen so the name describes the car's behaviour rather than decorating it.

| Name | Arabic | Meaning | Why this car |
|---|---|---|---|
| **Sahm** | سهم | arrow | Fired in a straight line. Matches "brutal straight-line speed, rewards commitment". |
| **Raqs** | رقص | dance | A car that dances. The drift car, where the slide is the point. |
| **Hajar** | حجر | stone | Rugged, grounded, unstoppable. Also the Al Hajar mountains — apt for the gravel car. |

**Why not wind names**, which are the obvious choice for fast cars: that space is heavily
occupied. Maserati alone has used Khamsin, Shamal, Bora and Merak; Volkswagen has Scirocco,
and Jeep has a Sahara trim. Picking a wind name would have been the fastest route to an
accidental collision.

All three are single words, ASCII-clean, and contain no apostrophes or diacritics — so they
work unmodified as asset name suffixes (`DA_VehicleSetup_Sahm`), Gameplay Tag leaves, and
directory names, with no escaping anywhere in the pipeline.

### Still outstanding — one check I cannot do

I checked these against car manufacturers and models I know of and found no collision. **I
cannot run a trademark search.** Before these names appear in a shipped build or on a
portfolio page, run each through a trademark register for automotive classes. That is a
five-minute check and it is the difference between an original name and an expensive one.

The known near-misses, so you know what was already considered and cleared:

- **Sahm** — a German glassware company, not automotive. No car.
- **Raqs** — no automotive use found.
- **Hajar** — a mountain range and a personal name. No car.

### Concept file names deliberately not renamed

The reference images stay as `concept_hyper.png` / `concept_gt.png` / `concept_rally.png`
rather than being renamed to the car names. Class-based filenames survive a name change; if
you decide Raqs should be something else, the file does not become misleading and the ledger
in `docs/ASSET_LICENCES.md` does not need editing.

### A fictional marque is a separate decision

These are **model names only.** Real racing games pair a marque with a model
("<Marque> Raqs"). This slice does not need one — the HUD and results screen show the model
name alone — but if you want one, it is one more original name subject to the same
trademark check, and it is a bigger collision risk than a model name because manufacturer
marks are defended far more aggressively.
