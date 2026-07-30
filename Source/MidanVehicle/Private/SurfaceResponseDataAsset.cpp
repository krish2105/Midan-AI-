#include "SurfaceResponseDataAsset.h"
#include "MidanGameplayTags.h"

#define LOCTEXT_NAMESPACE "SurfaceResponseDataAsset"

const FSurfaceResponseRow* USurfaceResponseDataAsset::FindRow(EPhysicalSurface InSurface) const
{
	for (const FSurfaceResponseRow& Row : Surfaces)
	{
		if (Row.PhysicalSurface == InSurface)
		{
			return &Row;
		}
	}

	// Fall back to the first row rather than returning null. A wheel over
	// unmapped geometry then behaves like the default surface instead of losing
	// all grip, which is the difference between a visual oddity and the car
	// falling through the world. Validation flags the unmapped surface
	// separately so this fallback never hides an authoring gap.
	return Surfaces.IsEmpty() ? nullptr : &Surfaces[0];
}

const FSurfaceResponseRow* USurfaceResponseDataAsset::FindRowByTag(const FGameplayTag& InTag) const
{
	for (const FSurfaceResponseRow& Row : Surfaces)
	{
		if (Row.SurfaceTag == InTag)
		{
			return &Row;
		}
	}
	return nullptr;
}

FPrimaryAssetId USurfaceResponseDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("MidanSurfaceResponse"), GetFName());
}

void USurfaceResponseDataAsset::ValidateMidanData(FMidanValidationResult& Result) const
{
	if (Surfaces.IsEmpty())
	{
		Result.AddError(TEXT("Surfaces"),
			LOCTEXT("NoSurfaces", "No surface rows defined. Off-track detection and surface-dependent grip both read from this asset, so an empty table disables both."));
		return;
	}

	// Every surface tag declared in MidanGameplayTags must have a row. A missing
	// row means a wheel on that surface silently falls back to the default, and
	// the rally car's whole reason to exist is that gravel differs from tarmac.
	const TArray<FGameplayTag> RequiredTags = {
		MidanTags::Surface_Tarmac,
		MidanTags::Surface_Kerb,
		MidanTags::Surface_Gravel,
		MidanTags::Surface_Grass,
		MidanTags::Surface_Sand,
		MidanTags::Surface_Wet
	};

	for (const FGameplayTag& Required : RequiredTags)
	{
		if (!FindRowByTag(Required))
		{
			Result.AddError(TEXT("Surfaces"), FText::Format(
				LOCTEXT("MissingSurfaceRowFmt", "No row for '{0}', which is declared in MidanGameplayTags.h and in Config/DefaultEngine.ini. A wheel on this surface would fall back to the default response."),
				FText::FromName(Required.GetTagName())));
		}
	}

	TSet<FGameplayTag> SeenTags;
	TSet<uint8> SeenSurfaces;

	for (int32 Index = 0; Index < Surfaces.Num(); ++Index)
	{
		const FSurfaceResponseRow& Row = Surfaces[Index];
		const FName Context(*FString::Printf(TEXT("Surfaces[%d]"), Index));

		if (!Row.SurfaceTag.IsValid())
		{
			Result.AddError(Context, LOCTEXT("RowNoTag", "Surface row has no tag."));
			continue;
		}

		if (!Row.SurfaceTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Surface"), false)))
		{
			// Requested with bErrorIfNotFound=false so a missing root tag reports
			// here rather than asserting.
			Result.AddWarning(Context, FText::Format(
				LOCTEXT("RowTagNotSurfaceFmt", "Tag '{0}' does not descend from Surface."),
				FText::FromName(Row.SurfaceTag.GetTagName())));
		}

		if (SeenTags.Contains(Row.SurfaceTag))
		{
			Result.AddError(Context, FText::Format(
				LOCTEXT("DuplicateTagFmt", "Duplicate row for '{0}'. FindRowByTag returns the first match, so the later row is dead configuration that appears to be active."),
				FText::FromName(Row.SurfaceTag.GetTagName())));
		}
		SeenTags.Add(Row.SurfaceTag);

		const uint8 SurfaceValue = static_cast<uint8>(Row.PhysicalSurface.GetValue());
		if (SeenSurfaces.Contains(SurfaceValue))
		{
			Result.AddError(Context, FText::Format(
				LOCTEXT("DuplicateSurfaceFmt", "Physical surface type {0} is already mapped by another row. Two tags cannot share one surface type — the second is unreachable."),
				FText::AsNumber(SurfaceValue)));
		}
		SeenSurfaces.Add(SurfaceValue);

		if (Row.FrictionMultiplier <= 0.f)
		{
			Result.AddError(Context, LOCTEXT("FrictionNonPositive", "Friction multiplier must be positive."));
		}

		// Tarmac is the 1.0 baseline every other surface is judged against.
		if (Row.SurfaceTag == MidanTags::Surface_Tarmac
			&& !FMath::IsNearlyEqual(Row.FrictionMultiplier, 1.f, 0.05f))
		{
			Result.AddWarning(Context, FText::Format(
				LOCTEXT("TarmacNotBaselineFmt", "Tarmac friction is {0}, not the 1.0 baseline. Every other surface is authored relative to tarmac, so changing it rescales the whole table implicitly — adjust the tyre FrictionForceMultiplier instead."),
				FText::AsNumber(Row.FrictionMultiplier)));
		}

		// Off-track surfaces must actually be off-track, or corner-cutting
		// through the gravel becomes a legal line.
		const bool bShouldBeOffTrack =
			Row.SurfaceTag == MidanTags::Surface_Gravel ||
			Row.SurfaceTag == MidanTags::Surface_Grass ||
			Row.SurfaceTag == MidanTags::Surface_Sand;

		if (bShouldBeOffTrack && Row.bCountsAsOnTrack)
		{
			Result.AddWarning(Context, FText::Format(
				LOCTEXT("OffTrackMarkedOnFmt", "'{0}' is marked as counting on-track. Lap validation would then accept a line cut across it."),
				FText::FromName(Row.SurfaceTag.GetTagName())));
		}

		if (Row.SurfaceTag == MidanTags::Surface_Kerb && !Row.bCountsAsOnTrack)
		{
			Result.AddWarning(Context,
				LOCTEXT("KerbOffTrack", "Kerb is marked off-track, so clipping a kerb on a normal racing line would start the lap-invalidation timer."));
		}
	}

	// A gravel/tarmac spread this narrow makes surface choice irrelevant, which
	// removes the rally car's purpose.
	const FSurfaceResponseRow* Tarmac = FindRowByTag(MidanTags::Surface_Tarmac);
	const FSurfaceResponseRow* Gravel = FindRowByTag(MidanTags::Surface_Gravel);
	if (Tarmac && Gravel && Tarmac->FrictionMultiplier > KINDA_SMALL_NUMBER)
	{
		const float Spread = Gravel->FrictionMultiplier / Tarmac->FrictionMultiplier;
		if (Spread > 0.9f)
		{
			Result.AddWarning(TEXT("Surfaces"), FText::Format(
				LOCTEXT("SurfaceSpreadNarrowFmt", "Gravel grip is {0} of tarmac. With so little difference, surface choice stops mattering and the rally car loses the character it exists for."),
				FText::AsNumber(Spread)));
		}
	}
}

#undef LOCTEXT_NAMESPACE
