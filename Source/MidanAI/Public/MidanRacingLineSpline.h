// The AI's driving line: a spline separate from the road centreline, carrying
// per-point target speed, braking-zone, and lateral-offset-bound data.
//
// Responsibility: where the AI drives and how fast, as authored/generated data.
// Single reason to change: how racing-line data is stored or sampled changes.
//
// A SEPARATE spline from AMidanTrackSpline, deliberately (docs/ARCHITECTURE.md
// §3.4) — the racing line cuts across the road's own centre through every
// corner, which is the entire point of a racing line. It carries its own
// curvature (computed from its own geometry, not the road's) because that is
// what MidanSpeedProfileGenerator needs: the curvature the AI will actually
// drive through, not the curvature of the road it happens to be on.
//
// MidanAI depends on MidanVehicle already, but this class deliberately does
// NOT depend on MidanRace — even though a level naturally has exactly one
// racing line the same way it has exactly one track spline. It is found by
// AMidanOpponentController via TActorIterator at Possess time, the same
// pattern AMidanRaceGameMode uses to find AMidanGridSpline, rather than
// through the Core service locator — a racing line is an MidanAI-internal
// concept with no other module needing to resolve one.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MidanRacingLineData.h"
#include "MidanRacingLineSpline.generated.h"

class USplineComponent;

UCLASS(Blueprintable)
class MIDANAI_API AMidanRacingLineSpline : public AActor
{
	GENERATED_BODY()

public:
	AMidanRacingLineSpline();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Midan|RacingLine", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> Spline;

	/** Index-aligned with Spline's keys. Generated (ReferenceTargetSpeedKmh,
	 *  bBrakingZone) or hand-authored (LateralOffsetMinCm/MaxCm) — see
	 *  MidanRacingLineToolLibrary and docs/MANUAL_STEPS.md. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Midan|RacingLine")
	TArray<FRacingLinePoint> Points;

	float GetLineLength() const;

	/** Signed curvature at an arc length along THIS spline, 1/cm — the input
	 *  MidanSpeedProfileGenerator consumes. Same finite-difference method as
	 *  AMidanTrackSpline::GetCurvatureAtDistance; duplicated rather than
	 *  shared because sharing it would mean either this class depending on
	 *  MidanRace for a ~10-line static function, or the function moving to
	 *  MidanCore for two callers that otherwise have nothing in common. */
	float GetCurvatureAtDistance(float Distance) const;

	/** Interpolated sample at an arbitrary arc length. Distance wraps. Empty
	 *  Points returns a zeroed sample at the spline's own geometry. */
	FRacingLineSample GetSampleAtDistance(float Distance) const;

	/** Nearest arc length on this line to a world location. Used by the
	 *  opponent controller to find "where am I on my own line" every frame,
	 *  and by MidanRubberBandComponent's gap estimate. */
	float GetClosestDistanceToWorldLocation(const FVector& WorldLocation) const;

	/** Replace the generated fields (ReferenceTargetSpeedKmh, bBrakingZone) on
	 *  every point, leaving hand-authored LateralOffsetMinCm/MaxCm untouched.
	 *  Called by MidanRacingLineToolLibrary after running
	 *  MidanSpeedProfileGenerator. Arrays must be the same length as Points. */
	void ApplyGeneratedProfile(const TArray<float>& TargetSpeedKmh, const TArray<bool>& BrakingZone);

private:
	float WrapDistance(float Distance) const;

	static constexpr float CurvatureSampleStepCm = 50.f;
};
