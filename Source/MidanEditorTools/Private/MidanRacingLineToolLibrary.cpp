#include "MidanRacingLineToolLibrary.h"

#include "Components/SplineComponent.h"
#include "MidanLogChannels.h"
#include "MidanRacingLineSpline.h"
#include "MidanSpeedProfileGenerator.h"

bool UMidanRacingLineToolLibrary::GenerateRacingLineSpeedProfile(
	AMidanRacingLineSpline* RacingLine,
	float MuEffective,
	float VMaxKmh,
	float MaxBrakingDecelerationCmS2)
{
	if (!RacingLine || !RacingLine->Spline)
	{
		UE_LOG(LogMidanAI, Error, TEXT("GenerateRacingLineSpeedProfile: RacingLine is null."));
		return false;
	}

	const int32 KeyCount = RacingLine->Spline->GetNumberOfSplinePoints();
	if (KeyCount < 3)
	{
		UE_LOG(LogMidanAI, Error, TEXT("GenerateRacingLineSpeedProfile: RacingLine has %d spline points; need at least 3."), KeyCount);
		return false;
	}

	if (RacingLine->Points.Num() != KeyCount)
	{
		UE_LOG(LogMidanAI, Error,
			TEXT("GenerateRacingLineSpeedProfile: Points.Num() (%d) does not match spline key count (%d). Resize Points to match the spline before generating."),
			RacingLine->Points.Num(), KeyCount);
		return false;
	}

	TArray<MidanSpeedProfile::FCurvatureSample> Samples;
	Samples.Reserve(KeyCount);

	for (int32 i = 0; i < KeyCount; ++i)
	{
		const float ArcLengthCm = RacingLine->Spline->GetDistanceAlongSplineAtSplinePoint(i);

		MidanSpeedProfile::FCurvatureSample Sample;
		Sample.ArcLengthCm = ArcLengthCm;
		Sample.Curvature = RacingLine->GetCurvatureAtDistance(ArcLengthCm);
		Samples.Add(Sample);
	}

	TArray<float> SpeedKmh;
	TArray<bool> BrakingZone;
	MidanSpeedProfile::GenerateSpeedProfile(
		Samples, MuEffective, VMaxKmh, MaxBrakingDecelerationCmS2,
		RacingLine->Spline->IsClosedLoop(), RacingLine->GetLineLength(),
		SpeedKmh, BrakingZone);

	RacingLine->ApplyGeneratedProfile(SpeedKmh, BrakingZone);

	int32 BrakingZoneCount = 0;
	for (bool bIsBraking : BrakingZone)
	{
		BrakingZoneCount += bIsBraking ? 1 : 0;
	}

	UE_LOG(LogMidanAI, Log, TEXT("GenerateRacingLineSpeedProfile: generated %d points over %.0fcm, mu=%.2f, VMax=%.0fkm/h, %d flagged as braking zones."),
		KeyCount, RacingLine->GetLineLength(), MuEffective, VMaxKmh, BrakingZoneCount);

	return true;
}
