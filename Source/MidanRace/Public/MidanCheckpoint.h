// One checkpoint gate: index, sector, arc length, half-width, and the overlap
// trigger that reports a crossing.
//
// Responsibility: detect a racer passing through this gate and broadcast it.
// Single reason to change: how a crossing is detected changes.
//
// Checkpoints, not trigger volumes standing in for off-track detection — the
// two are different problems. Off-track state comes from the physical
// material under each wheel (master prompt §1.2, read via
// IMidanVehicleInterface). Checkpoints exist purely to sequence lap and sector
// progress and to give the respawn system a "last valid position". A vehicle
// can be off-track and still crossing checkpoints correctly (a shortcut inside
// the racing surface's own width), which is exactly why the two systems are
// separate and why corner-cutting rejection lives in the checkpoint sequence,
// not in the surface sensor.
//
// Generated deterministically by Tools/editor_python/batch_setup_checkpoints.py
// via MidanCheckpointGeneratorLibrary, named Checkpoint_00, Checkpoint_01, ...

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MidanCheckpoint.generated.h"

class UBoxComponent;
class AMidanCheckpoint;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCheckpointCrossed, AMidanCheckpoint* /*Checkpoint*/, AActor* /*Racer*/);

UCLASS(Blueprintable)
class MIDANRACE_API AMidanCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	AMidanCheckpoint();

	/** Position in the lap sequence, 0 is the finish line. Must be unique and
	 *  contiguous across all checkpoints in a level — UMidanLapTimingSubsystem
	 *  asserts this at BeginPlay rather than inferring order from world
	 *  position, so a checkpoint accidentally dragged out of place fails loudly
	 *  instead of silently reordering the lap sequence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Midan|Checkpoint", meta = (ClampMin = "0"))
	int32 Index = 0;

	/** Which sector this checkpoint begins. Sector N's split time is recorded
	 *  when the first checkpoint carrying SectorIndex N+1 is crossed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Midan|Checkpoint", meta = (ClampMin = "0"))
	int32 SectorIndex = 0;

	/** Arc length along the track spline, cm. Authored redundantly with world
	 *  placement (rather than derived from it) so the lap timing subsystem can
	 *  sort and validate the sequence without a track reference. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Midan|Checkpoint")
	float ArcLength = 0.f;

	/** Half the drivable width at this arc length. Sizes the trigger and is
	 *  the position the respawn system snaps back to, offset laterally within
	 *  this bound rather than dead-centre, so a respawn does not de-rank a
	 *  racer who was legitimately running wide. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Midan|Checkpoint", meta = (ClampMin = "50.0"))
	float HalfWidth = 600.f;

	/** Broadcast once per overlap with an actor implementing
	 *  IMidanVehicleInterface. Deliberately not a UPROPERTY delegate — this is
	 *  an internal wiring point for UMidanLapTimingSubsystem, not something a
	 *  designer binds from a Blueprint graph (CLAUDE.md: no gameplay logic in
	 *  Blueprint). */
	FOnCheckpointCrossed OnCheckpointCrossed;

	//~ AActor
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Midan|Checkpoint", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> TriggerBox;
};
