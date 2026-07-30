// Predictive sphere traces along the projected path. Blends a lateral offset
// rather than braking where possible.
//
// Responsibility: detect an imminent collision and produce a way around it.
// Single reason to change: the prediction or avoidance model changes.
//
// Attached to AMidanOpponentController, not the pawn — avoidance is a driving
// DECISION (the same category as overtaking and rubber-banding), not a
// vehicle system. Traces at AMidanAIDifficultyDataAsset::AvoidanceTraceInterval
// -Seconds rather than every frame: this is the expensive part of the AI
// control loop (a handful of world sphere traces per racer), and per-racer
// staggering (docs/ARCHITECTURE.md §2.3, "the stagger keeps game-thread cost
// flat") happens by randomising each instance's first-fire time in BeginPlay,
// so seven opponents' trace ticks land on different frames rather than all
// spiking together.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MidanAvoidanceComponent.generated.h"

class UMidanAIDifficultyDataAsset;

UCLASS(ClassGroup = (Midan))
class MIDANAI_API UMidanAvoidanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMidanAvoidanceComponent();

	void Initialise(const UMidanAIDifficultyDataAsset* Difficulty);

	/** Lateral offset, cm, to add to the racing-line aim point this frame.
	 *  0 when the path ahead is clear. Cheap: reads state cached by the last
	 *  trace, does not itself trace. */
	float GetAvoidanceOffsetCm() const { return CurrentOffsetCm; }

	/** 0..1: how hard to brake in addition to the speed-profile target,
	 *  because a lateral dodge alone cannot resolve a predicted collision in
	 *  time (something dead ahead, closing fast). Usually 0 — avoidance
	 *  prefers steering to braking per the class comment. */
	float GetEmergencyBrakeFactor() const { return CurrentEmergencyBrakeFactor; }

	//~ UActorComponent
	virtual void BeginPlay() override;

private:
	void PerformTrace();

	/** Prediction horizon, seconds. Fixed at the master prompt's specified
	 *  1.5s — this is a spec value, not a tuning value; a designer changing
	 *  it would be changing what "predictive" means for this system, not
	 *  tuning how it drives. */
	static constexpr float PredictionHorizonSeconds = 1.5f;

	/** Trace sphere radius, cm — roughly a car's half-width plus margin.
	 *  Structural (approximates vehicle geometry for the sensor), not a
	 *  per-difficulty dial. */
	static constexpr float TraceSphereRadiusCm = 120.f;

	/** How far left/right the two side probes sit when choosing which way
	 *  to dodge, cm. */
	static constexpr float SideProbeOffsetCm = 200.f;

	FTimerHandle TraceTimerHandle;

	float TraceIntervalSeconds = 0.15f;
	float MaxOffsetCm = 250.f;

	float CurrentOffsetCm = 0.f;
	float CurrentEmergencyBrakeFactor = 0.f;
};
