// Rewinds a stuck or flipped racer to its last valid checkpoint.
//
// Responsibility: restore a racer's transform and physics state to a safe,
// valid point on track, on a cooldown.
// Single reason to change: what counts as a valid respawn point, or the
// restoration procedure, changes.
//
// Lives in MidanRace, not MidanVehicle: it is attached to the vehicle
// Blueprint (composition, which CLAUDE.md permits — the rule forbids
// gameplay LOGIC in Blueprint, not adding a component) so that a car
// respawns without MidanVehicle depending on MidanRace or vice versa. It
// talks to its owner only through IMidanVehicleInterface (input lock during
// the respawn flash) and generic AActor/USceneComponent API for the physics
// reset — nothing here names AMidanVehiclePawn.
//
// The safe point is the last checkpoint UMidanLapTimingSubsystem accepted for
// this racer (GetRacerLastCheckpointArcLength) — a known-safe waypoint, not
// the nearest point on the centreline to wherever the racer currently is,
// which could be right back at the hazard it just left.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MidanRespawnComponent.generated.h"

class UMidanRaceRulesDataAsset;
class UInputAction;
class UInputMappingContext;

UCLASS(ClassGroup = (Midan), meta = (BlueprintSpawnableComponent))
class MIDANRACE_API UMidanRespawnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMidanRespawnComponent();

	/** Cooldown and grace numbers. Soft, async-loaded — same pattern as every
	 *  other Data Asset reference in this project. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|Respawn")
	TSoftObjectPtr<UMidanRaceRulesDataAsset> RaceRules;

	/** Bound directly by this component when the owner is locally controlled,
	 *  so a respawn request needs no wiring through the vehicle's own input
	 *  component (which does not know this component exists — see the class
	 *  comment). Author IA_Respawn per docs/MANUAL_STEPS.md 1.3. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|Respawn")
	TSoftObjectPtr<UInputAction> RespawnAction;

	/**
	 * Request a respawn now.
	 *
	 * No-ops silently if on cooldown or if no valid checkpoint has been
	 * recorded yet (the opening seconds of a race, before the first
	 * checkpoint crossing). Callable from Blueprint so a Phase 7 HUD prompt
	 * or debug console can trigger it without a new C++ entry point.
	 */
	UFUNCTION(BlueprintCallable, Category = "Midan|Respawn")
	void RequestRespawn();

	/** Not a Tick-driven countdown — see docs/ARCHITECTURE.md §2.3's tick
	 *  inventory, which this component is deliberately absent from. Cooldown
	 *  is a stamped end time compared against world time on demand, so
	 *  checking it costs nothing between requests. */
	UFUNCTION(BlueprintPure, Category = "Midan|Respawn")
	bool IsOnCooldown() const;

	UFUNCTION(BlueprintPure, Category = "Midan|Respawn")
	float GetCooldownRemainingSeconds() const;

	//~ UActorComponent
	virtual void BeginPlay() override;

private:
	void OnRulesLoaded();
	void OnRespawnActionLoaded();
	void PerformRespawn();

	/** Binds RespawnAction on the owner's InputComponent, if the owner is a
	 *  locally-controlled pawn. Called only after the soft action reference
	 *  resolves — CLAUDE.md forbids a synchronous load here too. */
	void TryBindRespawnInput();

	TSharedPtr<struct FStreamableHandle> RulesLoadHandle;
	TSharedPtr<struct FStreamableHandle> RespawnActionLoadHandle;

	UPROPERTY(Transient)
	TObjectPtr<const UMidanRaceRulesDataAsset> LoadedRaceRules;

	/** World time (GetWorld()->GetTimeSeconds()) the cooldown ends. -1 means
	 *  never requested / not on cooldown. */
	float CooldownEndsAtWorldTimeSeconds = -1.f;
};
