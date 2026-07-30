#include "VehicleSurfaceSensorComponent.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "MidanLogChannels.h"
#include "SurfaceResponseDataAsset.h"

UVehicleSurfaceSensorComponent::UVehicleSurfaceSensorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVehicleSurfaceSensorComponent::BeginPlay()
{
	Super::BeginPlay();

	// API VERIFY: see VehicleAeroComponent::BeginPlay — same async physics opt-in.
	SetAsyncPhysicsTickEnabled(true);
}

void UVehicleSurfaceSensorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear the cached raw row pointers before the asset can be collected. They
	// point into CachedSurfaceAsset's TArray, so they must not outlive it.
	for (int32 i = 0; i < MidanVehicleConstants::NumWheels; ++i)
	{
		Samples[i] = FWheelSurfaceSample();
	}
	FMemory::Memzero(SurfaceLookup, sizeof(SurfaceLookup));
	DefaultRow = nullptr;
	bInitialised = false;

	Super::EndPlay(EndPlayReason);
}

void UVehicleSurfaceSensorComponent::InitialiseFromAsset(
	UChaosWheeledVehicleMovementComponent* InMovement,
	const USurfaceResponseDataAsset* InSurfaceAsset)
{
	Movement = InMovement;
	CachedSurfaceAsset = InSurfaceAsset;

	FMemory::Memzero(SurfaceLookup, sizeof(SurfaceLookup));
	DefaultRow = nullptr;

	if (!InSurfaceAsset || InSurfaceAsset->Surfaces.Num() == 0)
	{
		UE_LOG(LogMidanVehicle, Error,
			TEXT("VehicleSurfaceSensorComponent on '%s': no surface response asset. Every wheel will "
				 "report an unknown surface, off-track detection will never fire, and surface-dependent "
				 "handling will not exist."),
			*GetNameSafe(GetOwner()));
		bInitialised = false;
		return;
	}

	// Build the flat lookup. Iterating the asset once here means the callback
	// never searches.
	for (const FSurfaceResponseRow& Row : InSurfaceAsset->Surfaces)
	{
		const int32 Index = static_cast<int32>(Row.PhysicalSurface.GetValue());
		if (Index >= 0 && Index < MaxPhysicalSurfaces)
		{
			if (SurfaceLookup[Index] != nullptr)
			{
				UE_LOG(LogMidanVehicle, Warning,
					TEXT("Surface asset '%s' maps EPhysicalSurface %d twice. Keeping the first row (%s). "
						 "Duplicate mappings are an authoring error."),
					*GetNameSafe(InSurfaceAsset), Index, *SurfaceLookup[Index]->SurfaceTag.ToString());
			}
			else
			{
				SurfaceLookup[Index] = &Row;
			}
		}
	}

	// First authored row is the fallback. A wheel on an unmapped material then
	// behaves like the primary racing surface rather than like a void, which is
	// the safer failure: an untagged material reads as slightly wrong grip
	// instead of the car sliding as though on ice.
	DefaultRow = &InSurfaceAsset->Surfaces[0];

	bInitialised = (Movement != nullptr);

	if (!bInitialised)
	{
		UE_LOG(LogMidanVehicle, Error,
			TEXT("VehicleSurfaceSensorComponent on '%s': no movement component."), *GetNameSafe(GetOwner()));
	}
}

void UVehicleSurfaceSensorComponent::AsyncPhysicsTickComponent(const float DeltaTime, const float SimTime)
{
	Super::AsyncPhysicsTickComponent(DeltaTime, SimTime);

	if (!bInitialised)
	{
		return;
	}

	UChaosWheeledVehicleMovementComponent* Move = Movement.Get();
	if (!Move)
	{
		return;
	}

	const int32 WheelCount = FMath::Min(Move->Wheels.Num(), MidanVehicleConstants::NumWheels);

	for (int32 i = 0; i < WheelCount; ++i)
	{
		FWheelSurfaceSample& Sample = Samples[i];

		// API VERIFY: the exact accessor for per-wheel contact state and the
		// contact physical material on UChaosWheeledVehicleMovementComponent in
		// 5.8 needs confirming — candidates are GetWheelState(i), the
		// FWheelStatus struct, and UChaosVehicleWheel::GetContactSurfaceMaterial.
		// Recorded under docs/ASSUMPTIONS.md A21. The LOGIC below is what matters
		// and does not change with the accessor's spelling.
		const FWheelStatus& Status = Move->GetWheelState(i);

		Sample.bInContact = Status.bInContact;
		Sample.SuspensionCompression = Status.NormalizedSuspensionLength;

		if (!Sample.bInContact)
		{
			// Airborne. Empty tag, null row, not off-track.
			//
			// Deliberately NOT off-track: a car mid-jump has not left the circuit,
			// and treating airtime as off-track would invalidate a lap for
			// crossing a crest. Phase 5's off-track logic only counts wheels that
			// are touching something.
			Sample.Row = nullptr;
			Sample.SurfaceTag = FGameplayTag();
			Sample.bOffTrack = false;
			continue;
		}

		// Resolve the physical material to a row via the flat lookup.
		const UPhysicalMaterial* ContactMaterial = Status.PhysMaterial.Get();
		const EPhysicalSurface Surface = ContactMaterial
			? ContactMaterial->SurfaceType.GetValue()
			: SurfaceType_Default;

		const int32 SurfaceIndex = static_cast<int32>(Surface);
		const FSurfaceResponseRow* Row =
			(SurfaceIndex >= 0 && SurfaceIndex < MaxPhysicalSurfaces && SurfaceLookup[SurfaceIndex] != nullptr)
				? SurfaceLookup[SurfaceIndex]
				: DefaultRow;

		Sample.Row = Row;
		Sample.SurfaceTag = Row ? Row->SurfaceTag : FGameplayTag();
		Sample.bOffTrack = Row ? !Row->bCountsAsOnTrack : false;
	}
}

FGameplayTag UVehicleSurfaceSensorComponent::GetWheelSurfaceTag(const int32 WheelIndex) const
{
	return Samples[FMath::Clamp(WheelIndex, 0, MidanVehicleConstants::NumWheels - 1)].SurfaceTag;
}

const FSurfaceResponseRow* UVehicleSurfaceSensorComponent::GetWheelSurfaceRow(const int32 WheelIndex) const
{
	return Samples[FMath::Clamp(WheelIndex, 0, MidanVehicleConstants::NumWheels - 1)].Row;
}

float UVehicleSurfaceSensorComponent::GetWheelFrictionMultiplier(const int32 WheelIndex) const
{
	const FSurfaceResponseRow* Row = GetWheelSurfaceRow(WheelIndex);
	// 1.0 rather than 0.0 when airborne: a caller multiplying grip by this should
	// get "unchanged", not "none".
	return Row ? Row->FrictionMultiplier : 1.f;
}

bool UVehicleSurfaceSensorComponent::IsWheelOffTrack(const int32 WheelIndex) const
{
	return Samples[FMath::Clamp(WheelIndex, 0, MidanVehicleConstants::NumWheels - 1)].bOffTrack;
}

int32 UVehicleSurfaceSensorComponent::GetOffTrackWheelCount() const
{
	int32 Count = 0;
	for (int32 i = 0; i < MidanVehicleConstants::NumWheels; ++i)
	{
		Count += Samples[i].bOffTrack ? 1 : 0;
	}
	return Count;
}

float UVehicleSurfaceSensorComponent::GetAverageRoughness() const
{
	float Total = 0.f;
	int32 Grounded = 0;

	for (int32 i = 0; i < MidanVehicleConstants::NumWheels; ++i)
	{
		if (Samples[i].bInContact && Samples[i].Row)
		{
			Total += Samples[i].Row->Roughness;
			++Grounded;
		}
	}

	// Zero when fully airborne, which is correct — air is smooth.
	return (Grounded > 0) ? (Total / static_cast<float>(Grounded)) : 0.f;
}

void UVehicleSurfaceSensorComponent::WriteToFrameState(FMidanVehicleFrameState& OutState) const
{
	for (int32 i = 0; i < MidanVehicleConstants::NumWheels; ++i)
	{
		FMidanWheelState& Wheel = OutState.Wheels[i];
		Wheel.SurfaceTag = Samples[i].SurfaceTag;
		Wheel.bInContact = Samples[i].bInContact;
		Wheel.bOffTrack = Samples[i].bOffTrack;
		Wheel.SuspensionCompression = Samples[i].SuspensionCompression;
	}
}
