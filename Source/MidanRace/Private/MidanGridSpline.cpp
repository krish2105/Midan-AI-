#include "MidanGridSpline.h"

#include "Components/SplineComponent.h"

AMidanGridSpline::AMidanGridSpline()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = Spline;
	Spline->SetClosedLoop(false);
	Spline->SetMobility(EComponentMobility::Static);
}

float AMidanGridSpline::GetLateralOffsetForSlot(int32 SlotIndex) const
{
	// Even slots right of centre, odd slots left — the conventional
	// staggered-grid pattern. 0 offset collapses this to a single-file grid.
	return (SlotIndex % 2 == 0) ? LateralStaggerOffset : -LateralStaggerOffset;
}

FTransform AMidanGridSpline::GetSlotTransform(int32 SlotIndex) const
{
	if (!Spline)
	{
		return FTransform::Identity;
	}

	const int32 ClampedIndex = FMath::Clamp(SlotIndex, 0, FMath::Max(0, SlotCount - 1));
	const float Distance = FMath::Min(ClampedIndex * SlotSpacing, Spline->GetSplineLength());

	FTransform SlotTransform = Spline->GetTransformAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

	const FVector Right = Spline->GetRightVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	SlotTransform.AddToTranslation(Right * GetLateralOffsetForSlot(ClampedIndex));

	return SlotTransform;
}
