# Midan — Asset Licences

Every third-party asset in this project is recorded here, on the same commit that adds the
asset. An asset without a row is a licensing liability, and "I'll document it later" is how
a portfolio piece becomes unpublishable.

**Currently empty — no third-party assets have been added.**

---

## Ledger

| Asset | Path in `Content/` | Source | Licence | Author | Date added | Modified? |
|---|---|---|---|---|---|---|
| *(none yet)* | | | | | | |

---

## Rules

1. **One row per asset or per coherent asset pack.** A Megascans cliff collection is one
   row; six unrelated Sketchfab meshes are six rows.
2. **Record the licence, not the site.** "Sketchfab" is not a licence. "CC0 1.0" and
   "CC-BY 4.0" are.
3. **CC-BY requires visible attribution in the shipped build.** The README's attribution
   section is generated from this table at Phase 10, so a missing row means a missing
   attribution means a licence breach.
4. **Record whether the asset was modified.** CC-BY-SA and some marketplace licences treat
   derivatives differently from verbatim use.
5. **No asset with an unclear licence enters the repo.** If the source does not state one,
   it is not usable. Absence of a stated licence is not permission.

## Vehicle assets — the legal boundary

Vehicles are **original fictional designs**. From a reference you may borrow *design
language* — proportions, a shoulder line, a general lamp arrangement — never geometry.

Forbidden regardless of the source asset's licence:

- Any manufacturer badge, name, or model name
- Any grille shape traceable to a specific marque
- Any light signature that functions as a brand identifier
- Any silhouette a car enthusiast could name

A CC0 licence on a mesh does **not** clear trade dress. Manufacturers enforce trade dress on
silhouette, grille, badge, and light signature independently of who holds copyright in the
model file. If a base mesh from Sketchfab is used as a starting point, it must be modified
substantially enough that the result is an original design, and this table records both the
source and the fact of modification.

See `docs/ART_DIRECTION.md` §0 for the full boundary.

## Expected sources for this project

| Source | Typical licence | Notes |
|---|---|---|
| Quixel Megascans | Free for use in UE | Rock, road surface, and scatter assets. The canyon walls are the easiest win in the reference frame |
| Epic sample projects (Vehicle Game, City Sample) | Epic Content Licence | Usable in UE projects; still gets a row |
| Sketchfab | Varies — check per asset | CC0 and CC-BY only. Never use a recognisable production car, whatever the licence says |
| freesound.org | Varies — CC0 and CC-BY | Engine layers, tyre scrub, impacts. Note that CC-BY audio needs attribution too |
| Self-authored | n/a | Record as "original" so the ledger is complete rather than sparse |
