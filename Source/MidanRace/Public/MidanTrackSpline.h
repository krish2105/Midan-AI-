// The circuit centreline. Implements IMidanTrackInterface and builds the
// spline-mesh road from a sparse list of authored sections.
//
// Responsibility: track geometry — length, transform-at-distance, curvature,
// width, banking, and the visual road mesh built from that geometry.
// Single reason to change: how track geometry is represented or meshed changes.
//
// Registers itself with UMidanServiceLocatorSubsystem on BeginPlay so MidanAI
// (Phase 6) and the rest of MidanRace can resolve a track without MidanRace
// exposing a hard class reference — see docs/ARCHITECTURE.md §2.1.
//
// Curvature is estimated by finite difference on the spline tangent rather
// than read from spline key data directly, because a designer-placed spline
// key's curvature is not simply queryable from USplineComponent and the
// AI speed profile generator (Phase 6) needs a signed, continuous value at an
// arbitrary distance, not just at keys.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MidanTrackInterface.h"
#include "MidanTrackSectionData.h"
#include "MidanTrackSpline.generated.h"

class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS(Blueprintable)
class MIDANRACE_API AMidanTrackSpline : public AActor, public IMidanTrackInterface
{
	GENERATED_BODY()

public:
	AMidanTrackSpline();

	/** The centreline. Public so level designers can edit spline points
	 *  directly in the viewport — geometry is authored data, not code. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Midan|Track", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> Spline;

	/**
	 * Sparse, sorted-by-StartDistance section list.
	 *
	 * Sorted ascending and validated rather than sorted silently on load — a
	 * silent sort would change which distance range a section covers without
	 * the designer noticing. Call ValidateSections in the editor after
	 * reordering.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Midan|Track")
	TArray<FMidanTrackSection> Sections;

	/** Road mesh, tiled per spline-mesh segment. Soft — resolved only when
	 *  rebuilding the mesh in the editor, never at runtime, so a missing
	 *  reference cannot block a cooked build from loading. */
	UPROPERTY(EditAnywhere, Category = "Midan|Road")
	TSoftObjectPtr<UStaticMesh> RoadMesh;

	/** Materials indexed by FMidanTrackSection::MaterialIndex. */
	UPROPERTY(EditAnywhere, Category = "Midan|Road")
	TArray<TSoftObjectPtr<UMaterialInterface>> RoadMaterials;

	/** Length of one spline-mesh segment, cm. Smaller segments follow banking
	 *  and curvature transitions more closely at the cost of more components;
	 *  this is a content-authoring trade, not gameplay tuning, so it stays a
	 *  plain editable property rather than a Data Asset field. */
	UPROPERTY(EditAnywhere, Category = "Midan|Road", meta = (ClampMin = "200.0", ClampMax = "5000.0"))
	float RoadSegmentLength = 1000.f;

	/** Regenerate the spline-mesh road from Sections. Editor-only: the mesh is
	 *  baked into the level, not rebuilt at runtime. */
	UFUNCTION(CallInEditor, Category = "Midan|Road")
	void RebuildRoadMesh();

	/** True when Sections is non-empty, sorted ascending by StartDistance, and
	 *  starts at 0. Logged via LogMidanRace rather than asserted, so a bad
	 *  section list is visible without crashing the editor. */
	UFUNCTION(CallInEditor, Category = "Midan|Track")
	bool ValidateSections() const;

	/** Section covering a given arc length. Distance wraps. Null only when
	 *  Sections is empty, which ValidateSections flags as invalid. */
	const FMidanTrackSection* GetSectionAtDistance(float Distance) const;

	//~ IMidanTrackInterface
	virtual float GetTrackLength() const override;
	virtual FTransform GetTransformAtDistance(float Distance) const override;
	virtual float GetClosestDistanceToWorldLocation(const FVector& WorldLocation, float& OutLateralOffset) const override;
	virtual float GetCurvatureAtDistance(float Distance) const override;
	virtual float GetTrackHalfWidthAtDistance(float Distance) const override;

	//~ AActor
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
#endif

private:
	/** Wraps Distance into [0, GetTrackLength()). A closed-loop circuit means a
	 *  lookahead past the finish line needs no special case at the caller —
	 *  IMidanTrackInterface::GetTransformAtDistance documents this contract. */
	float WrapDistance(float Distance) const;

	/** Finite-difference step for curvature estimation, cm. Small enough to
	 *  approximate a derivative, large enough to stay well clear of spline
	 *  floating-point degeneracy at consecutive keys. Not tuning — it is a
	 *  numerical-method constant with no design meaning. */
	static constexpr float CurvatureSampleStepCm = 50.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> RoadMeshSegments;
};
