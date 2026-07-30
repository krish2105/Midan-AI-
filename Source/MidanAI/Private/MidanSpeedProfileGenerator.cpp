#include "MidanSpeedProfileGenerator.h"

#include "MidanMathUtils.h"

void MidanSpeedProfile::ComputeCorneringLimit(
	const TArray<FCurvatureSample>& Samples,
	float MuEffective,
	float VMaxKmh,
	TArray<float>& OutCorneringLimitKmh)
{
	OutCorneringLimitKmh.SetNumUninitialized(Samples.Num());

	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		const float AbsCurvature = FMath::Abs(Samples[i].Curvature);

		if (AbsCurvature <= KINDA_SMALL_NUMBER)
		{
			// A straight: not grip-limited at all, so the ceiling is VMax.
			OutCorneringLimitKmh[i] = VMaxKmh;
			continue;
		}

		// v = sqrt(mu * g / |curvature|), both in cm-based units, then to km/h.
		const float VCmS = FMath::Sqrt(MuEffective * MidanMath::GravityCmS2 / AbsCurvature);
		OutCorneringLimitKmh[i] = FMath::Min(VCmS * MidanMath::CmSToKmH, VMaxKmh);
	}
}

void MidanSpeedProfile::ApplyBackwardBrakingPass(
	const TArray<FCurvatureSample>& Samples,
	const TArray<float>& CorneringLimitKmh,
	float MaxBrakingDecelerationCmS2,
	bool bClosedLoop,
	float TrackLengthCm,
	TArray<float>& OutSpeedKmh)
{
	const int32 N = Samples.Num();
	OutSpeedKmh = CorneringLimitKmh;

	if (N < 2 || MaxBrakingDecelerationCmS2 <= 0.f)
	{
		return;
	}

	const int32 PassCount = bClosedLoop ? 2 : 1;

	for (int32 Pass = 0; Pass < PassCount; ++Pass)
	{
		for (int32 i = N - 1; i >= 0; --i)
		{
			const bool bIsLastSample = (i == N - 1);
			if (bIsLastSample && !bClosedLoop)
			{
				// Nothing downstream to brake toward on an open sequence.
				continue;
			}

			const int32 NextIndex = bIsLastSample ? 0 : (i + 1);

			float DistanceCm = Samples[NextIndex].ArcLengthCm - Samples[i].ArcLengthCm;
			if (bIsLastSample)
			{
				// Wrap across the finish line.
				DistanceCm = (TrackLengthCm - Samples[i].ArcLengthCm) + Samples[NextIndex].ArcLengthCm;
			}

			if (DistanceCm <= 0.f)
			{
				continue;
			}

			const float ExitSpeedCmS = OutSpeedKmh[NextIndex] * MidanMath::KmHToCmS;
			const float MaxEntrySpeedCmS = FMath::Sqrt(
				FMath::Square(ExitSpeedCmS) + 2.f * MaxBrakingDecelerationCmS2 * DistanceCm);

			OutSpeedKmh[i] = FMath::Min(OutSpeedKmh[i], MaxEntrySpeedCmS * MidanMath::CmSToKmH);
		}
	}
}

void MidanSpeedProfile::GenerateSpeedProfile(
	const TArray<FCurvatureSample>& Samples,
	float MuEffective,
	float VMaxKmh,
	float MaxBrakingDecelerationCmS2,
	bool bClosedLoop,
	float TrackLengthCm,
	TArray<float>& OutSpeedKmh,
	TArray<bool>& OutBrakingZone)
{
	TArray<float> CorneringLimitKmh;
	ComputeCorneringLimit(Samples, MuEffective, VMaxKmh, CorneringLimitKmh);
	ApplyBackwardBrakingPass(Samples, CorneringLimitKmh, MaxBrakingDecelerationCmS2, bClosedLoop, TrackLengthCm, OutSpeedKmh);

	OutBrakingZone.SetNumUninitialized(OutSpeedKmh.Num());
	for (int32 i = 0; i < OutSpeedKmh.Num(); ++i)
	{
		OutBrakingZone[i] = OutSpeedKmh[i] < (CorneringLimitKmh[i] - KINDA_SMALL_NUMBER);
	}
}
