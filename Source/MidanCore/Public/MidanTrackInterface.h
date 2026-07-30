// What a track must expose.
//
// Responsibility: the contract between the track and everything that measures
// progress along it.
// Single reason to change: a consumer needs track geometry it cannot derive.
//
// Declared in Phase 3 rather than Phase 5 because UMidanServiceLocatorSubsystem
// holds a TScriptInterface to it and cannot compile against a forward
// declaration alone (docs/ASSUMPTIONS.md A26). The implementation,
// AMidanTrackSpline, still lands at Phase 5.
//
// Everything here is const and geometric. A track answers questions about
// itself; it does not own race rules, timing, or vehicle state.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MidanTrackInterface.generated.h"

UINTERFACE(MinimalAPI)
class UMidanTrackInterface : public UInterface
{
	GENERATED_BODY()
};

class MIDANCORE_API IMidanTrackInterface
{
	GENERATED_BODY()

public:
	/** Total closed-loop length, cm. */
	virtual float GetTrackLength() const = 0;

	/** World transform at a distance along the centreline. Distance wraps, so a
	 *  lookahead past the finish line needs no special case at the caller. */
	virtual FTransform GetTransformAtDistance(float Distance) const = 0;

	/**
	 * Nearest point on the centreline to a world location.
	 *
	 * Returns arc length; writes the signed lateral offset (positive right of
	 * travel direction). This is how a vehicle finds itself on the track, so it
	 * is called for eight vehicles at 10Hz and must not be expensive.
	 */
	virtual float GetClosestDistanceToWorldLocation(const FVector& WorldLocation, float& OutLateralOffset) const = 0;

	/**
	 * Signed curvature at a distance, 1/cm. Positive turns right.
	 *
	 * The AI speed profile generator is the primary consumer:
	 * v_target = sqrt(mu * g / |curvature|). Curvature near zero means a
	 * straight, so callers must guard the division rather than relying on a
	 * clamped return.
	 */
	virtual float GetCurvatureAtDistance(float Distance) const = 0;

	/** Drivable half-width at a distance, cm. Bounds the AI's lateral offset
	 *  lanes and the off-track grace region. */
	virtual float GetTrackHalfWidthAtDistance(float Distance) const = 0;
};
