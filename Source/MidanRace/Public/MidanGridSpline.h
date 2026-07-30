// Data-driven starting grid: 8 slots (player + 7 AI), staggered left/right.
//
// Responsibility: where each racer starts and which way it faces.
// Single reason to change: grid geometry or slot count changes.
//
// Assumption A14 in docs/ASSUMPTIONS.md: 8 slots, never specified upstream
// beyond "seven AI opponents". A spline rather than 8 hand-placed
// PlayerStarts, so moving or re-angling the whole grid is one spline edit
// instead of eight, and slot spacing stays data (SlotSpacing, LateralOffset)
// rather than baked into individually placed actors.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MidanGridSpline.generated.h"

class USplineComponent;

UCLASS(Blueprintable)
class MIDANRACE_API AMidanGridSpline : public AActor
{
	GENERATED_BODY()

public:
	AMidanGridSpline();

	/** Runs from the front slot (index 0, pole position) backward. Straight or
	 *  gently curved to follow the track's start straight. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Midan|Grid", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> Spline;

	/** Number of grid slots. Assumption A14: player + seven AI. Not a
	 *  MidanRaceConstants value — unlike wheel count or sector count, a
	 *  different field size (a smaller field, a time-trial single-car grid)
	 *  is a plausible future mode, not a structural rewrite, so it stays an
	 *  editable count rather than a compiled-in constant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Midan|Grid", meta = (ClampMin = "1", ClampMax = "32"))
	int32 SlotCount = 8;

	/** Distance along the spline between consecutive slots, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Midan|Grid", meta = (ClampMin = "300.0"))
	float SlotSpacing = 800.f;

	/** Lateral offset from spline centre, cm. Slots alternate left/right by
	 *  this amount for a staggered grid; 0 disables staggering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Midan|Grid", meta = (ClampMin = "0.0"))
	float LateralStaggerOffset = 300.f;

	/** World transform for a grid slot. Slot 0 is pole position, at the
	 *  spline's start. Index is clamped into [0, SlotCount).
	 *  Called by AMidanRaceGameMode when populating the grid. */
	UFUNCTION(BlueprintPure, Category = "Midan|Grid")
	FTransform GetSlotTransform(int32 SlotIndex) const;

private:
	float GetLateralOffsetForSlot(int32 SlotIndex) const;
};
