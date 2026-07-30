// Lateral offset lanes: evaluates a lane change on closing-speed and gap
// thresholds, with a predictive clear sweep before committing.
//
// Responsibility: decide whether to pass a car ahead, and pick which side.
// Single reason to change: the overtake decision model changes.
//
// Attached to AMidanOpponentController, same reasoning as
// UMidanAvoidanceComponent — this is a driving decision, not a vehicle
// system. Detects candidates entirely through world overlap queries filtered
// by IMidanVehicleInterface, never through MidanRace's position/lap data —
// MidanAI does not depend on MidanRace (docs/ASSUMPTIONS.md A9, A10), and a
// physical proximity check answers "is there a car in front of me" without
// needing to.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MidanOvertakeComponent.generated.h"

class UMidanAIDifficultyDataAsset;

UCLASS(ClassGroup = (Midan))
class MIDANAI_API UMidanOvertakeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMidanOvertakeComponent();

	void Initialise(const UMidanAIDifficultyDataAsset* Difficulty);

	/** Lateral offset, cm, to add to the racing-line aim point this frame.
	 *  0 when not attempting a pass. */
	float GetOvertakeOffsetCm() const { return CurrentOffsetCm; }

	//~ UActorComponent
	virtual void BeginPlay() override;

private:
	void EvaluateOvertake();

	/** Lateral separation the maneuver aims for once committed, cm — enough
	 *  to be clearly alongside rather than tucked in, but bounded by the
	 *  racing line's own LateralOffsetMinCm/MaxCm at the controller, which
	 *  clamps whatever this component asks for. Structural (a car-width-plus-
	 *  margin estimate), not a per-difficulty dial — Aggression affects
	 *  WHETHER and WHEN to commit, not how far to move once committed. */
	static constexpr float CommittedLateralOffsetCm = 350.f;

	/** Once committed, the gap must open past this multiple of
	 *  OvertakeMinGapSeconds before aborting — prevents a pass that flickers
	 *  on and off every check interval as the gap hovers near the threshold. */
	static constexpr float AbortGapMultiplier = 1.6f;

	FTimerHandle CheckTimerHandle;

	float CheckIntervalSeconds = 0.5f;
	float MinGapSeconds = 1.5f;
	float DetectionRangeCm = 3000.f;
	float Aggression = 0.5f;

	float CurrentOffsetCm = 0.f;
	bool bCommitted = false;
	TWeakObjectPtr<AActor> TargetActor;
};
