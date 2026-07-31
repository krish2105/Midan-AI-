# Midan — Build Runbook

The complete path from a clean checkout to a verified, uploaded Shipping build. Every
script referenced here lives in `Tools/build/` and is documented in this repo, not only in
this file — run any of them with `--help` for the authoritative flag list.

**Status: this runbook has never been executed.** No engine is installed in this
environment (`docs/MANUAL_STEPS.md` §0.1) and Phase 10's own gate requires a physical
machine. Every command below is the intended, reviewed procedure — not a confirmed one.
Treat a first run as a first run: expect to correct flag names and paths against the
installed engine's actual behaviour, the same discipline every other phase's gate applied.

---

## 1. Prerequisites

- UE 5.8 installed (`docs/MANUAL_STEPS.md` §0.1).
- Xcode (Mac) or Visual Studio 2022 with "Game development with C++" (Windows).
- `git`, `git-lfs` configured (`git lfs install` once per machine).
- Python 3.11+ for every `Tools/` script.
- For itch.io distribution: `butler` on `PATH`, `butler login` run once interactively.
- For CI: a self-hosted runner, labelled `midan-mac` (active) or `midan-win64` (gated —
  `.github/workflows/ci.yml`'s Win64 jobs stay `if: false` until one exists).

## 2. Local development loop

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh" \
    -project="$PWD/Midan.uproject" -game -engine

"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" \
    MidanEditor Mac Development -project="$PWD/Midan.uproject"
```

Then open `Midan.uproject` in the editor as usual.

## 3. Data validation and Automation Specs — run before every commit

```bash
"<UE>/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/Midan.uproject" -run=DataValidation

"<UE>/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/Midan.uproject" \
    -run=Automation -test="Midan" -log
```

Both are non-negotiable per-commit gates in CI (`.github/workflows/ci.yml`'s
`data-validation` and `automation-specs` jobs) — a red Automation Spec or a failed
validator blocks everything downstream, including the functional tests and any Shipping
package.

## 4. Functional tests (map-based, `MidanTests` module)

```bash
"<UE>/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/Midan.uproject" \
    /Game/Midan/Maps/L_MidanCircuit -run=FunctionalTest -unattended -nosplash
```

Requires the four `AFunctionalTest` actors from `Source/MidanTests/` placed on a test map
(`docs/MANUAL_STEPS.md` Phase 10 §10.1) — a vehicle spawn test, an AI lap-completion test,
a respawn test, and a full race-completion test. `MidanTests` never links into a Shipping
target (`Source/Midan.Target.cs`), so this step only ever runs against Development or Test.

## 5. Build a single platform target

```bash
python3 Tools/build/build.py --platform Mac --configuration Test
```

`--configuration` is `Development`, `Test`, or `Shipping`. Use `Test` for anything a
performance number will be reported from — CLAUDE.md's measurement rule requires it, never
`Development`.

## 6. The full Shipping chain — one command

```bash
python3 Tools/build/package_shipping.py --platform Mac
```

Runs, in order, and stops at the first failure (`docs/PHASE_PLAN.md` Phase 10: "do not
proceed if a hard gate fails — fix it first"):

1. `build.py --configuration Shipping`
2. `generate_pso_cache.py` — three collection playthroughs (Day, Night, Deep Night —
   `docs/ASSUMPTIONS.md` A22/A23), merged into the cache bundled with the build
3. `verify_build.py` — the six hard gates from `docs/PHASE_PLAN.md` Phase 10

## 7. Verify an existing build independently

```bash
python3 Tools/build/verify_build.py --platform Mac --build-dir Builds/Mac
```

Prints PASS/FAIL per gate by name — see the script's own docstring for the six gates.
Exits non-zero on any failure, so it composes into a CI step or a pre-upload check without
extra parsing.

## 8. Upload to itch.io

```bash
python3 Tools/build/upload_itch.py --platform Mac --build-dir Builds/Mac \
    --project <you>/midan --channel mac-test
```

Refuses to upload unless `verify_build.py` has just passed against the same build — see
the script's docstring for the one, explicit exception (`--skip-verify`, internal channels
only).

## 9. Measuring — the numbers this runbook produces

Two documents receive numbers FROM this pipeline, never numbers invented for it:

- `docs/PERFORMANCE_BUDGET.md` — frame budget, from `Tools/analysis/perf_report.py`
  (Phase 8) against a **Test** configuration build.
- `README.md`'s performance table — package size, cold launch time, and **hitch count
  before and after PSO caching** (`generate_pso_cache.py`'s explicit before/after numbers),
  from a **Shipping** configuration build, per `docs/PHASE_PLAN.md` Phase 10's gate.

Every number in either document carries all five provenance fields from
`docs/PERFORMANCE_BUDGET.md` §4: build configuration, resolution/TSR screen percentage,
machine and GPU, scalability tier, and run count. A number missing any of the five is not
a number this project reports.

## 10. CI overview

`.github/workflows/ci.yml`, self-hosted only (`docs/PHASE_PLAN.md` Phase 10: "keep the
DDC shared and warm — a cold DDC turns a 10-minute build into 90"):

| Job | Runs on | Trigger |
|---|---|---|
| `compile-mac` | `midan-mac` | every PR and push to `main` |
| `data-validation` | `midan-mac` | after compile |
| `automation-specs` | `midan-mac` | after compile |
| `functional-tests` | `midan-mac` | after compile |
| `package-shipping-mac` | `midan-mac` | push to `main` only, after all three above pass |
| `compile-win64` / `package-shipping-win64` | `midan-win64` | **gated off** (`if: false`) until a Windows runner exists |

## 11. Known gaps

- **Nothing in this runbook has been executed.** No engine, no hardware. Every command is
  the reviewed procedure, not a confirmed one — expect a first-run correction pass, same as
  every other phase's first compile.
- **PSO cache expansion (`ShaderPipelineCacheTools Expand`) is a Windows-oriented
  toolchain path** (`docs/ASSUMPTIONS.md` A19). `generate_pso_cache.py` still produces the
  three raw per-preset collection files on Mac even if the merge step needs the Win64 lane
  to complete.
- **`verify_build.py`'s pak-integrity and main-menu-smoke-test gates are minimum-viability
  checks**, not full `UnrealPak -test` CRC verification or a real "reached the main menu"
  signal — both are stated as such in the script's own output rather than overclaiming.
