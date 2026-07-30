// Python-callable wrapper over MidanSpeedProfileGenerator.
//
// Responsibility: sample a racing line's own curvature and write a generated
// speed profile back onto it.
// Single reason to change: how curvature is sampled from the spline changes.
//
// Thin by design, same reasoning as MidanCheckpointGeneratorLibrary
// (docs/ARCHITECTURE.md §3.6): the math lives in MidanAI where
// SpeedProfileSpec already exercises it; this wrapper only owns turning a
// live AMidanRacingLineSpline into the sample array that math needs.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MidanRacingLineToolLibrary.generated.h"

class AMidanRacingLineSpline;

UCLASS()
class UMidanRacingLineToolLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Sample RacingLine's own curvature at SampleSpacingCm intervals, run
	 * MidanSpeedProfileGenerator, and write the result onto every
	 * FRacingLinePoint via AMidanRacingLineSpline::ApplyGeneratedProfile.
	 *
	 * Samples at spline KEY positions specifically (not just evenly-spaced
	 * arc lengths) — Points is index-aligned with spline keys
	 * (AMidanRacingLineSpline's own contract), so the generator's output
	 * array must be too, or ApplyGeneratedProfile's length check rejects it.
	 *
	 * Returns false (and applies nothing) if RacingLine is null or has fewer
	 * than 3 spline points.
	 */
	UFUNCTION(BlueprintCallable, Category = "Midan|Editor|AI")
	static bool GenerateRacingLineSpeedProfile(
		AMidanRacingLineSpline* RacingLine,
		float MuEffective,
		float VMaxKmh,
		float MaxBrakingDecelerationCmS2);
};
