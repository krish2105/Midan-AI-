// Routes UMidanDataAsset::ValidateMidanData into the editor Data Validation
// framework, so `-run=DataValidation` can fail a CI build on a bad asset.
//
// Responsibility: adapt the runtime validation contract to the editor framework.
// Single reason to change: the UEditorValidatorBase interface changes.
//
// One validator covers every tuning asset in the project. That is deliberate:
// because all of them derive from UMidanDataAsset and implement the same hook,
// adding a new asset type needs no new validator and cannot be forgotten. A
// per-type validator would make "someone added an asset type and forgot to
// register it" a silent gap in coverage.
//
// This header is in Private/ — nothing outside this module should include it.

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "MidanDataValidators.generated.h"

UCLASS()
class UMidanDataAssetValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	UMidanDataAssetValidator();

protected:
	//~ Begin UEditorValidatorBase interface
	virtual bool CanValidateAsset_Implementation(
		const FAssetData& InAssetData,
		UObject* InAsset,
		FDataValidationContext& InContext) const override;

	virtual EDataValidationResult ValidateLoadedAsset_Implementation(
		const FAssetData& InAssetData,
		UObject* InAsset,
		FDataValidationContext& InContext) override;
	//~ End UEditorValidatorBase interface
};
