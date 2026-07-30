#include "MidanCheckpoint.h"

#include "Components/BoxComponent.h"
#include "MidanVehicleInterface.h"

namespace
{
	/** Trigger depth along the direction of travel, cm. Thin: a checkpoint is
	 *  a plane, not a zone — a deep trigger would let a racer register a
	 *  crossing while braking through it, which double-counts near a chicane
	 *  with two checkpoints close together. */
	static constexpr float TriggerDepthCm = 200.f;

	/** Trigger height, cm. Tall enough to catch a car airborne over a crest. */
	static constexpr float TriggerHeightCm = 400.f;
}

AMidanCheckpoint::AMidanCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(TriggerDepthCm, HalfWidth, TriggerHeightCm));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->SetGenerateOverlapEvents(true);
}

void AMidanCheckpoint::BeginPlay()
{
	Super::BeginPlay();

	// Resize now: HalfWidth may have been set by the checkpoint generator
	// after construction, and this is the last point before triggers go live.
	TriggerBox->SetBoxExtent(FVector(TriggerDepthCm, HalfWidth, TriggerHeightCm));

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMidanCheckpoint::HandleBeginOverlap);
}

void AMidanCheckpoint::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Filtered by interface, not by class — MidanRace never includes a
	// MidanVehicle header, so "is this a racer" can only be asked through
	// IMidanVehicleInterface, declared in MidanCore.
	if (OtherActor && OtherActor->Implements<UMidanVehicleInterface>())
	{
		OnCheckpointCrossed.Broadcast(this, OtherActor);
	}
}
