---
name: unreal-cpp-architecture
description: Use when writing, reviewing, or restructuring Unreal Engine C++ — modules and .Build.cs/.Target.cs wiring, UCLASS/USTRUCT/UPROPERTY declarations, subsystems, components, interfaces, Data Assets, gameplay tags, or deciding what belongs in C++ versus a Data Asset versus a Blueprint. Trigger for "add a module", "new UCLASS", "circular dependency", "should this be a subsystem", "editor-only code", "where does this value live". Do NOT trigger for engine-agnostic C++ or for editor content work that cannot be expressed in code.
---

# Unreal C++ Architecture

Unreal punishes architectural mistakes late. A module dependency you added in week 1
becomes unremovable by week 8; a tuning float you inlined in C++ kills the data pipeline
that made the project defensible. This skill is the set of decisions worth making before
the first `UCLASS`.

---

## 1. The three-layer split — decide this before anything else

| Lives in | Contains | Never contains |
|---|---|---|
| **C++** | Systems, algorithms, components, subsystems, interfaces, math | Tuning numbers |
| **Data Assets** | Every value a human would tune by feel | Logic |
| **Blueprint** | Thin subclasses, asset references, cosmetic wiring | Systems, math, gameplay rules |

**The test:** would a designer ever want to change this value without a programmer
present? Then it is a Data Asset field. There is no middle category.

**Enforce it structurally, not by discipline.** Discipline erodes under deadline. Give
every tuning asset a common base with a mandatory validation hook:

```cpp
UCLASS(Abstract)
class MYPROJ_API UProjDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    /** Return false and fill OutErrors when this asset is physically nonsensical. */
    virtual bool ValidateData(TArray<FText>& OutErrors) const { return true; }
};
```

Then route it into the editor's Data Validation framework from an editor-only module, and
CI can run `-run=DataValidation` and fail on a bad asset. Now the rule is a build gate
rather than a code-review preference.

**The permitted exception:** a `constexpr` with a comment stating *why it can never
change*. Physical constants qualify. "Feels about right" does not.

```cpp
// Standard gravity. Not a tuning value — changing it makes every physics
// number in every Data Asset meaningless.
static constexpr float GravityCmS2 = 980.f;
```

---

## 2. Module design

### 2.1 Split by responsibility, not by layer

A module per subsystem-of-the-game (`Vehicle`, `Race`, `AI`) beats a module per technical
layer (`Actors`, `Components`, `Data`). Files that change together should compile
together.

Module count has a cost: each one is a `.Build.cs`, an export macro, a load phase, and a
link step. Under about five modules the overhead outweighs the isolation. Over about ten,
you are usually modelling folders as modules.

### 2.2 The dependency rule that actually matters

**A foundation module depends on nothing project-side.** Everything else depends on it,
and siblings never depend on each other.

Circular module dependencies do not merely warn — UBT refuses to link. And the fix at
week 8 is a refactor, not an edit.

When module B needs something from module A and A is not the foundation, you have three
options, in order of preference:

1. **Move the shared type into the foundation module.** Usually right for PODs, enums,
   and gameplay tags.
2. **Declare an interface in the foundation module**, have A implement it, and let B
   consume it. Right when B needs *behaviour*, not data.
3. **Add the dependency.** Right when the coupling is genuine and permanent. Say so
   explicitly rather than drifting into it.

Option 2 with a locator subsystem is the pattern that keeps modules independently
testable:

```cpp
// In the foundation module.
UCLASS()
class PROJCORE_API UProjServiceLocatorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void RegisterTrack(TScriptInterface<IProjTrackInterface> InTrack);
    TScriptInterface<IProjTrackInterface> GetTrack() const;
private:
    TScriptInterface<IProjTrackInterface> Track;
};
```

The cost is one indirection and registration at `BeginPlay`. The benefit is that the
consumer module compiles, and unit-tests, without the provider.

### 2.3 Editor-only modules

Editor tooling goes in a module with `Type = ModuleType.Editor`, listed in
`ProjEditor.Target.cs` and **absent from** `Proj.Target.cs`. That is stronger than
`#if WITH_EDITOR` scattered through runtime code: the symbols cannot reach a shipping
binary because they were never compiled into that target.

Keep editor modules thin. Put the algorithm in a runtime module where an Automation Spec
can reach it, and let the editor module be a `UBlueprintFunctionLibrary` wrapper that
Python can call. If the wrapper contains logic worth testing, the split is in the wrong
place.

### 2.4 `.Build.cs` hygiene

- `PublicDependencyModuleNames` only for modules whose types appear in your public
  headers. Everything else is `PrivateDependencyModuleNames`. Getting this wrong inflates
  every dependent module's include graph.
- `PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs`.
- Add a module to `.uproject` plugins list only when you actually need the plugin — each
  one costs load time and shader permutations.

---

## 3. Class-level conventions

### 3.1 Pick the right base

| Need | Use |
|---|---|
| Per-world service, no actor | `UWorldSubsystem` |
| Per-session service surviving level loads | `UGameInstanceSubsystem` |
| Behaviour attached to an actor | `UActorComponent` / `USceneComponent` |
| A thing in the world | `AActor` |
| Tuning data | `UPrimaryDataAsset` subclass |
| Project config that is not tuning | `UDeveloperSettings` |
| A contract across modules | `UINTERFACE` + `IInterface` |

Subsystems replaced the singleton pattern and the "manager actor placed in every level"
pattern. If you are writing `GetInstance()`, you want a subsystem.

### 3.2 Pointers

- `TObjectPtr<T>` for `UObject` members. Never a raw `UObject*` member.
- `TSoftObjectPtr<T>` for any asset not needed immediately — and **never** hard-reference
  an asset in a constructor. Constructors run during CDO creation; a hard reference there
  drags the asset into memory at startup and defeats streaming.
- `TWeakObjectPtr<T>` for back-references that must not keep the target alive.
- `TScriptInterface<T>` to hold an interface plus its `UObject`.

### 3.3 Headers

Forward-declare in headers, include in `.cpp`. Every unnecessary header include multiplies
across the module. Include what you use in the `.cpp`; do not lean on transitive includes,
because they break on engine upgrade.

### 3.4 Every class states its reason to change

A one-line header comment giving the class's responsibility **and its single reason to
change**. The second clause is the useful one — if you cannot name one reason, the class
does two things.

```cpp
/**
 * Applies a vehicle setup Data Asset onto the Chaos movement component.
 * Responsibility: data → physics translation, nothing else.
 * Single reason to change: the Chaos parameter surface changes.
 */
```

---

## 4. Gameplay tags over strings and enums

Native tags declared once, referenced everywhere:

```cpp
namespace ProjTags
{
    PROJCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Race_State_Countdown);
}
```

Never `FGameplayTag::RequestGameplayTag(FName("Race.State.Countdown"))` at a call site —
that is a string comparison wearing a costume, and it fails silently when the tag is
renamed.

Tags beat enums when the vocabulary will grow, when hierarchy matters
(`Surface.Gravel` matching `Surface`), or when designers need to add values. Enums are
still right for a closed set that code switches over exhaustively.

---

## 5. Performance-shaped decisions

### 5.1 Justify every tick

`Tick` is the default that quietly costs you the frame. Before adding one, ask whether a
timer, a delegate, or an existing callback will do.

| Cadence | Use when |
|---|---|
| Physics callback | Force application, anything needing substep coherence |
| Per-frame `Tick` | Camera, visual interpolation, audio parameters |
| Timer at N Hz | Position calculation, AI re-planning, anything a human cannot perceive at frame rate |
| Delegate / event | State changes, overlaps, input |

Keep a written tick inventory in the architecture doc, with a justification per entry.
Additions then require an argument rather than an edit.

### 5.2 Never allocate in a hot path

No `TArray` growth, no `FString` construction, no `NewObject`, no map insertion in a
physics callback or per-frame function. Preallocate at `BeginPlay` with `Reserve()` and
reuse. A ring buffer sized once beats a queue that grows.

`FString` formatting is a common accidental allocation — guard logging in hot paths behind
a verbosity check, or remove it.

### 5.3 Design for testability, and it shapes the code

Pure math in free functions or plain structs with no `UWorld` dependency can be tested by
an Automation Spec without a map. That is not only a testing convenience — it is usually
the better design. A PID controller does not need to be a component. A position
calculator does not need a world.

If a class needs a `UWorld` to test, ask what it is doing that requires one.

---

## 6. Review checklist

- [ ] No circular module dependency; foundation module depends on nothing project-side
- [ ] Cross-module reads go through foundation-declared interfaces, not sibling links
- [ ] Editor-only code lives in an `Editor`-type module absent from the game target
- [ ] Public vs private dependencies correctly split in every `.Build.cs`
- [ ] No tuning float in C++; every one is a Data Asset `UPROPERTY` or a justified `constexpr`
- [ ] Every tuning Data Asset overrides `ValidateData` and the validator is CI-reachable
- [ ] `TObjectPtr` members; `TSoftObjectPtr` for deferred assets; no hard asset ref in a constructor
- [ ] Native gameplay tags, no string-constructed tags at call sites
- [ ] Every tick justified and recorded in the tick inventory
- [ ] Zero allocation in physics callbacks and per-frame hot paths
- [ ] Pure math extracted to world-free functions or structs, with a spec
- [ ] Every class header names its responsibility and its single reason to change
- [ ] Forward-declared headers, includes in `.cpp`
