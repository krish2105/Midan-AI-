#include "MidanDataValidators.h"
#include "MidanDataAsset.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "MidanDataValidators"

UMidanDataAssetValidator::UMidanDataAssetValidator()
{
	bIsEnabled = true;   // API VERIFY: field name on UEditorValidatorBase in 5.8
}

bool UMidanDataAssetValidator::CanValidateAsset_Implementation(
	const FAssetData& InAssetData,
	UObject* InAsset,
	FDataValidationContext& InContext) const
{
	// One check covers every current and future tuning asset type.
	return InAsset && InAsset->IsA<UMidanDataAsset>();
}

EDataValidationResult UMidanDataAssetValidator::ValidateLoadedAsset_Implementation(
	const FAssetData& InAssetData,
	UObject* InAsset,
	FDataValidationContext& InContext)
{
	const UMidanDataAsset* Asset = Cast<UMidanDataAsset>(InAsset);
	if (!Asset)
	{
		return EDataValidationResult::NotValidated;
	}

	FMidanValidationResult Result;
	Asset->ValidateMidanData(Result);

	for (const FMidanValidationIssue& Issue : Result.Issues)
	{
		// Context is prefixed onto the message so the editor's validation log
		// points at the exact nested field — "Tyres.Rear.FrictionForceMultiplier"
		// rather than just the asset name.
		const FText Formatted = FText::Format(
			LOCTEXT("IssueFmt", "[{0}] {1}"),
			FText::FromName(Issue.Context),
			Issue.Message);

		if (Issue.bIsError)
		{
			AssetFails(InAsset, Formatted);
		}
		else
		{
			AssetWarning(InAsset, Formatted);
		}
	}

	if (Result.HasErrors())
	{
		return EDataValidationResult::Invalid;
	}

	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE
