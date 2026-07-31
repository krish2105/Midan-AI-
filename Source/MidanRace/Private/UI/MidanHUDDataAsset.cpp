#include "UI/MidanHUDDataAsset.h"

#define LOCTEXT_NAMESPACE "MidanHUDDataAsset"

void UMidanHUDDataAsset::ValidateMidanData(FMidanValidationResult& Result) const
{
	if (RPMAmberThreshold >= RPMRedThreshold)
	{
		Result.AddError(TEXT("RPMAmberThreshold"),
			LOCTEXT("BadRPMThresholds", "RPMAmberThreshold must be less than RPMRedThreshold."));
	}

	if (HUDScaleMin >= HUDScaleMax)
	{
		Result.AddError(TEXT("HUDScaleMin"),
			LOCTEXT("BadHUDScaleRange", "HUDScaleMin must be less than HUDScaleMax."));
	}

	if (HUDScaleMin > 1.0f || HUDScaleMax < 1.0f)
	{
		Result.AddWarning(TEXT("HUDScaleMin"),
			LOCTEXT("HUDScaleExcludesDefault", "The [HUDScaleMin, HUDScaleMax] range does not include 1.0 (the default scale). Confirm that is intentional."));
	}
}

#undef LOCTEXT_NAMESPACE
