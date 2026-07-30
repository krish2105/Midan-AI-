// Root of every tuning Data Asset in the project.
//
// Responsibility: give every tuning asset a mandatory, uniform validation hook.
// Single reason to change: the validation contract itself changes.
//
// Why this exists: CLAUDE.md forbids hardcoded tuning values, which means all
// of them live in Data Assets — and an unvalidated Data Asset is worse than a
// hardcoded constant, because a designer can set mass to zero and only find out
// when the physics solver produces NaN. MidanEditorTools routes ValidateData
// into the editor Data Validation framework so CI can run -run=DataValidation
// and fail the build. The rule is a build gate, not a code-review preference.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MidanDataAsset.generated.h"

/**
 * A single validation finding. Errors block; warnings inform.
 *
 * The distinction is load-bearing. A high centre of mass is intentional on the
 * rally car — it must warn, never error, or the validator would fight the
 * design. See .claude/skills/vehicle-physics-tuning §2.
 */
USTRUCT(BlueprintType)
struct MIDANCORE_API FMidanValidationIssue
{
	GENERATED_BODY()

	/** Property or sub-struct this concerns, for editor navigation. */
	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	FName Context;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	FText Message;

	/** True blocks cook and CI; false is advisory only. */
	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	bool bIsError = true;

	FMidanValidationIssue() = default;

	FMidanValidationIssue(FName InContext, FText InMessage, bool bInIsError)
		: Context(InContext)
		, Message(MoveTemp(InMessage))
		, bIsError(bInIsError)
	{
	}
};

/**
 * Collects validation findings. Passed down through nested config structs so
 * each one reports against its own context without knowing who called it.
 */
USTRUCT(BlueprintType)
struct MIDANCORE_API FMidanValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	TArray<FMidanValidationIssue> Issues;

	void AddError(FName Context, const FText& Message)
	{
		Issues.Emplace(Context, Message, true);
	}

	void AddWarning(FName Context, const FText& Message)
	{
		Issues.Emplace(Context, Message, false);
	}

	bool HasErrors() const
	{
		for (const FMidanValidationIssue& Issue : Issues)
		{
			if (Issue.bIsError)
			{
				return true;
			}
		}
		return false;
	}

	int32 CountErrors() const
	{
		int32 Count = 0;
		for (const FMidanValidationIssue& Issue : Issues)
		{
			Count += Issue.bIsError ? 1 : 0;
		}
		return Count;
	}

	int32 CountWarnings() const
	{
		return Issues.Num() - CountErrors();
	}
};

/**
 * Base class for every asset holding tuning values.
 *
 * Derive, override ValidateMidanData, and the editor validator picks it up
 * automatically — no registration per asset type.
 */
UCLASS(Abstract, BlueprintType)
class MIDANCORE_API UMidanDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Report every physically nonsensical or internally inconsistent value.
	 *
	 * Contract: append findings, never clear the result — nested config structs
	 * accumulate into one report so a designer sees all problems at once rather
	 * than fixing them one compile at a time.
	 */
	virtual void ValidateMidanData(FMidanValidationResult& Result) const
	{
	}

	/** Convenience wrapper. Returns false when any error was reported. */
	bool IsMidanDataValid(FMidanValidationResult& OutResult) const
	{
		ValidateMidanData(OutResult);
		return !OutResult.HasErrors();
	}
};
