#include "MidanRacingLineSpline.h"

#include "Components/SplineComponent.h"
#include "MidanLogChannels.h"

AMidanRacingLineSpline::AMidanRacingLineSpline()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = Spline;
	Spline->SetClosedLoop(true);
	Spline->SetMobility(EComponentMobility::Static);
}

float AMidanRacingLineSpline::WrapDistance(float Distance) const
{
	const float Length = GetLineLength();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	const float Wrapped = FMath::Fmod(Distance, Length);
	return Wrapped < 0.f ? Wrapped + Length : Wrapped;
}

float AMidanRacingLineSpline::GetLineLength() const
{
	return Spline ? Spline->GetSplineLength() : 0.f;
}

float AMidanRacingLineSpline::GetCurvatureAtDistance(float Distance) const
{
	if (!Spline || GetLineLength() <= CurvatureSampleStepCm * 2.f)
	{
		return 0.f;
	}

	const float D0 = WrapDistance(Distance);
	const float D1 = WrapDistance(Distance + CurvatureSampleStepCm);

	const FVector Dir0 = Spline->GetDirectionAtDistanceAlongSpline(D0, ESplineCoordinateSpace::World);
	const FVector Dir1 = Spline->GetDirectionAtDistanceAlongSpline(D1, ESplineCoordinateSpace::World);

	const float CrossZ = (Dir0.X * Dir1.Y) - (Dir0.Y * Dir1.X);
	const float Dot = FVector::DotProduct(Dir0, Dir1);
	const float AngleRadians = FMath::Atan2(CrossZ, Dot);

	return AngleRadians / CurvatureSampleStepCm;
}

FRacingLineSample AMidanRacingLineSpline::GetSampleAtDistance(float Distance) const
{
	FRacingLineSample Sample;

	if (!Spline)
	{
		return Sample;
	}

	const float Wrapped = WrapDistance(Distance);
	Sample.WorldLocation = Spline->GetLocationAtDistanceAlongSpline(Wrapped, ESplineCoordinateSpace::World);
	Sample.ForwardDirection = Spline->GetDirectionAtDistanceAlongSpline(Wrapped, ESplineCoordinateSpace::World);

	const int32 NumPoints = Points.Num();
	if (NumPoints == 0)
	{
		return Sample;
	}
	if (NumPoints == 1)
	{
		Sample.ReferenceTargetSpeedKmh = Points[0].ReferenceTargetSpeedKmh;
		Sample.bBrakingZone = Points[0].bBrakingZone;
		Sample.LateralOffsetMinCm = Points[0].LateralOffsetMinCm;
		Sample.LateralOffsetMaxCm = Points[0].LateralOffsetMaxCm;
		return Sample;
	}

	// Points is index-aligned with spline keys, so the fractional spline
	// input key IS the interpolation parameter between Points[i] and
	// Points[i+1] — no separate arc-length-to-point-index search needed.
	const float InputKey = Spline->GetInputKeyValueAtDistanceAlongSpline(Wrapped);
	const int32 IndexA = FMath::Clamp(FMath::FloorToInt(InputKey), 0, NumPoints - 1);
	const int32 IndexB = (IndexA + 1) % NumPoints;
	const float Alpha = FMath::Frac(InputKey);

	const FRacingLinePoint& A = Points[IndexA];
	const FRacingLinePoint& B = Points[IndexB];

	Sample.ReferenceTargetSpeedKmh = FMath::Lerp(A.ReferenceTargetSpeedKmh, B.ReferenceTargetSpeedKmh, Alpha);
	Sample.bBrakingZone = A.bBrakingZone; // discrete, not interpolated — belongs to the segment leaving A.
	Sample.LateralOffsetMinCm = FMath::Lerp(A.LateralOffsetMinCm, B.LateralOffsetMinCm, Alpha);
	Sample.LateralOffsetMaxCm = FMath::Lerp(A.LateralOffsetMaxCm, B.LateralOffsetMaxCm, Alpha);

	return Sample;
}

float AMidanRacingLineSpline::GetClosestDistanceToWorldLocation(const FVector& WorldLocation) const
{
	if (!Spline)
	{
		return 0.f;
	}
	const float Key = Spline->FindInputKeyClosestToWorldLocation(WorldLocation);
	return Spline->GetDistanceAlongSplineAtSplineInputKey(Key);
}

void AMidanRacingLineSpline::ApplyGeneratedProfile(const TArray<float>& TargetSpeedKmh, const TArray<bool>& BrakingZone)
{
	if (TargetSpeedKmh.Num() != Points.Num() || BrakingZone.Num() != Points.Num())
	{
		UE_LOG(LogMidanAI, Error, TEXT("'%s': ApplyGeneratedProfile array length mismatch (Points=%d, Speed=%d, Braking=%d). Nothing applied."),
			*GetName(), Points.Num(), TargetSpeedKmh.Num(), BrakingZone.Num());
		return;
	}

	for (int32 i = 0; i < Points.Num(); ++i)
	{
		Points[i].ReferenceTargetSpeedKmh = TargetSpeedKmh[i];
		Points[i].bBrakingZone = BrakingZone[i];
	}
}
