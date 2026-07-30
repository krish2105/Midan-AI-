#include "MidanTrackSpline.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MidanLogChannels.h"
#include "MidanServiceLocator.h"

AMidanTrackSpline::AMidanTrackSpline()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = Spline;
	Spline->SetClosedLoop(true);
	Spline->SetMobility(EComponentMobility::Static);
}

float AMidanTrackSpline::WrapDistance(float Distance) const
{
	const float Length = GetTrackLength();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	// FMath::Fmod can return negative for negative input; wrap into [0, Length).
	const float Wrapped = FMath::Fmod(Distance, Length);
	return Wrapped < 0.f ? Wrapped + Length : Wrapped;
}

float AMidanTrackSpline::GetTrackLength() const
{
	return Spline ? Spline->GetSplineLength() : 0.f;
}

FTransform AMidanTrackSpline::GetTransformAtDistance(float Distance) const
{
	if (!Spline)
	{
		return FTransform::Identity;
	}
	return Spline->GetTransformAtDistanceAlongSpline(WrapDistance(Distance), ESplineCoordinateSpace::World);
}

float AMidanTrackSpline::GetClosestDistanceToWorldLocation(const FVector& WorldLocation, float& OutLateralOffset) const
{
	if (!Spline)
	{
		OutLateralOffset = 0.f;
		return 0.f;
	}

	const float Key = Spline->FindInputKeyClosestToWorldLocation(WorldLocation);
	const float Distance = Spline->GetDistanceAlongSplineAtSplineInputKey(Key);
	const FVector ClosestLocation = Spline->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
	const FVector Right = Spline->GetRightVectorAtSplineInputKey(Key, ESplineCoordinateSpace::World);

	// Positive right of the direction of travel, per IMidanTrackInterface's
	// contract — the same sign convention FMidanTrackPosition::LateralOffset
	// documents in MidanCore.
	OutLateralOffset = FVector::DotProduct(WorldLocation - ClosestLocation, Right);
	return Distance;
}

float AMidanTrackSpline::GetCurvatureAtDistance(float Distance) const
{
	if (!Spline || GetTrackLength() <= CurvatureSampleStepCm * 2.f)
	{
		return 0.f;
	}

	const float D0 = WrapDistance(Distance);
	const float D1 = WrapDistance(Distance + CurvatureSampleStepCm);

	const FVector Dir0 = Spline->GetDirectionAtDistanceAlongSpline(D0, ESplineCoordinateSpace::World);
	const FVector Dir1 = Spline->GetDirectionAtDistanceAlongSpline(D1, ESplineCoordinateSpace::World);

	// Signed angle between the two tangents about the world-up axis.
	// API VERIFY: sign convention for "positive turns right" depends on the
	// spline's handedness as authored — confirm against a known right-hander
	// once a track exists (docs/ASSUMPTIONS.md).
	const float CrossZ = (Dir0.X * Dir1.Y) - (Dir0.Y * Dir1.X);
	const float Dot = FVector::DotProduct(Dir0, Dir1);
	const float AngleRadians = FMath::Atan2(CrossZ, Dot);

	return AngleRadians / CurvatureSampleStepCm;
}

const FMidanTrackSection* AMidanTrackSpline::GetSectionAtDistance(float Distance) const
{
	if (Sections.Num() == 0)
	{
		return nullptr;
	}

	const float Wrapped = WrapDistance(Distance);

	// Sections are authored sparse and sorted ascending; the section covering
	// Wrapped is the last one whose StartDistance does not exceed it. Linear
	// search is fine — a circuit has a handful of sections, not thousands, and
	// this is not called from the physics callback.
	const FMidanTrackSection* Found = nullptr;
	for (const FMidanTrackSection& Section : Sections)
	{
		if (Section.StartDistance > Wrapped)
		{
			break;
		}
		Found = &Section;
	}

	// Wrapped is before the first section's StartDistance: it belongs to the
	// last section, wrapping around the finish line.
	return Found ? Found : &Sections.Last();
}

float AMidanTrackSpline::GetTrackHalfWidthAtDistance(float Distance) const
{
	static constexpr float DefaultHalfWidth = 600.f;

	const FMidanTrackSection* Section = GetSectionAtDistance(Distance);
	if (!Section)
	{
		return DefaultHalfWidth;
	}
	return Section->Width * 0.5f;
}

bool AMidanTrackSpline::ValidateSections() const
{
	if (Sections.Num() == 0)
	{
		UE_LOG(LogMidanRace, Warning, TEXT("'%s': Sections is empty. GetTrackHalfWidthAtDistance will return a fallback width everywhere."), *GetName());
		return false;
	}

	bool bOk = true;
	for (int32 i = 1; i < Sections.Num(); ++i)
	{
		if (Sections[i].StartDistance <= Sections[i - 1].StartDistance)
		{
			UE_LOG(LogMidanRace, Warning,
				TEXT("'%s': Sections[%d].StartDistance (%.1f) does not exceed Sections[%d]'s (%.1f). Sections must be sorted strictly ascending."),
				*GetName(), i, Sections[i].StartDistance, i - 1, Sections[i - 1].StartDistance);
			bOk = false;
		}
	}

	if (!Sections.IsEmpty() && !FMath::IsNearlyZero(Sections[0].StartDistance))
	{
		UE_LOG(LogMidanRace, Warning, TEXT("'%s': Sections[0].StartDistance is %.1f, not 0. The first section should start at the finish line."), *GetName(), Sections[0].StartDistance);
	}

	return bOk;
}

void AMidanTrackSpline::RebuildRoadMesh()
{
#if WITH_EDITOR
	for (USplineMeshComponent* Segment : RoadMeshSegments)
	{
		if (Segment)
		{
			Segment->DestroyComponent();
		}
	}
	RoadMeshSegments.Reset();

	UStaticMesh* Mesh = RoadMesh.LoadSynchronous();
	if (!Mesh || !Spline)
	{
		UE_LOG(LogMidanRace, Warning, TEXT("'%s': RebuildRoadMesh needs RoadMesh set. No segments generated."), *GetName());
		return;
	}

	if (!ValidateSections())
	{
		UE_LOG(LogMidanRace, Warning, TEXT("'%s': generating road mesh despite section warnings above."), *GetName());
	}

	const float Length = GetTrackLength();
	const int32 SegmentCount = FMath::Max(1, FMath::RoundToInt(Length / RoadSegmentLength));
	const float ActualSegmentLength = Length / static_cast<float>(SegmentCount);

	for (int32 i = 0; i < SegmentCount; ++i)
	{
		const float StartDistance = i * ActualSegmentLength;
		const float EndDistance = (i + 1) * ActualSegmentLength;

		USplineMeshComponent* Segment = NewObject<USplineMeshComponent>(this, NAME_None, RF_Transactional);
		Segment->SetMobility(EComponentMobility::Static);
		Segment->SetStaticMesh(Mesh);
		Segment->SetupAttachment(Spline);

		const FVector StartLoc = Spline->GetLocationAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local);
		const FVector StartTangent = Spline->GetTangentAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local) / static_cast<float>(SegmentCount);
		const FVector EndLoc = Spline->GetLocationAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local);
		const FVector EndTangent = Spline->GetTangentAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local) / static_cast<float>(SegmentCount);

		// API VERIFY: SetStartAndEnd's tangent scaling convention on UE 5.8.
		Segment->SetStartAndEnd(StartLoc, StartTangent, EndLoc, EndTangent, false);

		if (const FMidanTrackSection* Section = GetSectionAtDistance(StartDistance))
		{
			if (RoadMaterials.IsValidIndex(Section->MaterialIndex))
			{
				if (UMaterialInterface* Material = RoadMaterials[Section->MaterialIndex].LoadSynchronous())
				{
					Segment->SetMaterial(0, Material);
				}
			}
		}

		Segment->RegisterComponent();
		RoadMeshSegments.Add(Segment);
	}

	UE_LOG(LogMidanRace, Log, TEXT("'%s': built %d road mesh segments over %.0fcm."), *GetName(), SegmentCount, Length);
#endif
}

#if WITH_EDITOR
void AMidanTrackSpline::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Deliberately NOT auto-rebuilding the mesh on every construction pass —
	// that would fire on every property edit in the details panel, including
	// ones unrelated to the road, and would fight a designer who is mid-edit
	// on Sections. RebuildRoadMesh is CallInEditor: an explicit action.
}
#endif

void AMidanTrackSpline::BeginPlay()
{
	Super::BeginPlay();

	if (UMidanServiceLocatorSubsystem* Locator = GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>())
	{
		Locator->RegisterTrack(TScriptInterface<IMidanTrackInterface>(this));
	}
}

void AMidanTrackSpline::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UMidanServiceLocatorSubsystem* Locator = World->GetSubsystem<UMidanServiceLocatorSubsystem>())
		{
			Locator->UnregisterTrack();
		}
	}

	Super::EndPlay(EndPlayReason);
}
