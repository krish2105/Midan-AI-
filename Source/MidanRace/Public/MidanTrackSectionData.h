// Per-section track geometry: width, banking, and surface material.
//
// Responsibility: the data a track section is built from.
// Single reason to change: a section gains a new authored property.
//
// Sections are authored as a sparse, sorted-by-distance list on
// AMidanTrackSpline rather than one entry per metre — a circuit built from a
// handful of hand-placed sections (start straight, hairpin, chicane, ...) is
// what a track designer actually authors, and AMidanTrackSpline resolves any
// arc length to the section that contains it.

#pragma once

#include "CoreMinimal.h"
#include "MidanTrackSectionData.generated.h"

/**
 * One authored stretch of track, holding constant from its StartDistance
 * until the next section's StartDistance (or the track length, for the last).
 */
USTRUCT(BlueprintType)
struct MIDANRACE_API FMidanTrackSection
{
	GENERATED_BODY()

	/** Arc length along the spline where this section begins, cm. Sections
	 *  must be sorted ascending by this field — AMidanTrackSpline validates it
	 *  rather than sorting silently, because a silent sort would move a
	 *  section's effective range without the designer noticing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Section")
	float StartDistance = 0.f;

	/** Drivable full width, cm, centred on the spline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Section", meta = (ClampMin = "100.0"))
	float Width = 1200.f;

	/** Banking, degrees, positive rolling the road right-side-up. Read by the
	 *  road mesh generator and available to gameplay for future lean-into-bank
	 *  effects; Phase 5 does not consume it beyond mesh generation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Section")
	float BankingDegrees = 0.f;

	/** Index into the road material's material-per-section array, so a
	 *  hairpin can swap to a coarser asphalt look without a second mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Section", meta = (ClampMin = "0"))
	int32 MaterialIndex = 0;
};
