#include "MidanRacingLineFollower.h"

#include "Components/SplineComponent.h"
#include "MidanAIDifficultyDataAsset.h"
#include "MidanRacingLineSpline.h"

void UMidanRacingLineFollower::Initialise(const AMidanRacingLineSpline* InRacingLine, const UMidanAIDifficultyDataAsset* InDifficulty)
{
	RacingLine = InRacingLine;

	if (InDifficulty)
	{
		SteeringResponseGain = InDifficulty->SteeringResponseGain;
		MinLookaheadDistanceCm = InDifficulty->MinLookaheadDistanceCm;
		MaxLookaheadDistanceCm = InDifficulty->MaxLookaheadDistanceCm;
		LookaheadSpeedNormalisationKmh = InDifficulty->LookaheadSpeedNormalisationKmh;
	}
}

float UMidanRacingLineFollower::ComputeLookaheadDistanceCm(float ForwardSpeedKmh) const
{
	const float SpeedAlpha = FMath::Clamp(FMath::Abs(ForwardSpeedKmh) / FMath::Max(LookaheadSpeedNormalisationKmh, 1.f), 0.f, 1.f);
	return FMath::Lerp(MinLookaheadDistanceCm, MaxLookaheadDistanceCm, SpeedAlpha);
}

float UMidanRacingLineFollower::GetArcLengthAtLocation(const FVector& VehicleLocation) const
{
	return RacingLine ? RacingLine->GetClosestDistanceToWorldLocation(VehicleLocation) : 0.f;
}

float UMidanRacingLineFollower::ComputeSteering(const FVector& VehicleLocation, const FVector& VehicleForward, float ForwardSpeedKmh, float LateralOffsetCm) const
{
	if (!RacingLine || !RacingLine->Spline)
	{
		return 0.f;
	}

	const float CurrentArc = GetArcLengthAtLocation(VehicleLocation);
	const float LookaheadCm = ComputeLookaheadDistanceCm(ForwardSpeedKmh);
	const float TargetArc = CurrentArc + LookaheadCm;

	const FRacingLineSample TargetSample = RacingLine->GetSampleAtDistance(TargetArc);

	// AMidanRacingLineSpline::Spline is a public UPROPERTY on a class this
	// module owns, so reaching for its right vector directly here is
	// preferable to duplicating the cross-product math FRacingLineSample
	// would otherwise need to carry for this one caller.
	const float WrappedTargetArc = FMath::Fmod(TargetArc, FMath::Max(RacingLine->GetLineLength(), 1.f));
	const FVector Right = RacingLine->Spline->GetRightVectorAtDistanceAlongSpline(
		WrappedTargetArc < 0.f ? WrappedTargetArc + RacingLine->GetLineLength() : WrappedTargetArc,
		ESplineCoordinateSpace::World);

	const FVector AimPoint = TargetSample.WorldLocation + Right * LateralOffsetCm;
	const FVector ToAimPoint = AimPoint - VehicleLocation;

	if (ToAimPoint.IsNearlyZero())
	{
		return 0.f;
	}

	const FVector Forward = VehicleForward.GetSafeNormal();
	const FVector ToAimPointNormalised = ToAimPoint.GetSafeNormal();

	// Signed heading angle to the aim point, positive = aim point is to the
	// right, using world-up (Z) as the rotation axis — same sign convention
	// AMidanTrackSpline::GetCurvatureAtDistance uses for "positive turns right".
	const float CrossZ = (Forward.X * ToAimPointNormalised.Y) - (Forward.Y * ToAimPointNormalised.X);
	const float Dot = FVector::DotProduct(Forward, ToAimPointNormalised);
	const float AlphaRadians = FMath::Atan2(CrossZ, Dot);

	// Classic pure-pursuit curvature is 2*sin(alpha)/L; used here as a
	// normalised-steering proxy rather than a literal curvature; L cancels
	// out of the proportionality and SteeringResponseGain is the tunable
	// replacement for it, so a designer can sharpen or soften the response
	// without it being coupled to lookahead distance.
	const float SteerNormalised = SteeringResponseGain * FMath::Sin(AlphaRadians);

	return FMath::Clamp(SteerNormalised, -1.f, 1.f);
}
