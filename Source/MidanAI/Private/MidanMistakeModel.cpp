#include "MidanMistakeModel.h"

void FMidanMistakeModel::OnEnteredBrakingZone(float MistakeProbability, float SpeedOvershootMultiplier, float LateralOffsetCm, float DurationSeconds)
{
	if (Rng.GetFRand() > FMath::Clamp(MistakeProbability, 0.f, 1.f))
	{
		return;
	}

	ActiveDurationSeconds = FMath::Max(DurationSeconds, KINDA_SMALL_NUMBER);
	RemainingSeconds = ActiveDurationSeconds;
	PeakSpeedOvershootMultiplier = FMath::Max(SpeedOvershootMultiplier, 1.f);

	// Random sign: a wide line can drift either way depending on the corner.
	const float Sign = (Rng.GetFRand() < 0.5f) ? -1.f : 1.f;
	PeakLateralOffsetCm = Sign * FMath::Abs(LateralOffsetCm);
}

void FMidanMistakeModel::Tick(float DeltaSeconds)
{
	RemainingSeconds = FMath::Max(0.f, RemainingSeconds - FMath::Max(DeltaSeconds, 0.f));
}

float FMidanMistakeModel::GetSpeedOvershootMultiplier() const
{
	if (!IsActive())
	{
		return 1.f;
	}
	const float Alpha = RemainingSeconds / ActiveDurationSeconds;
	return FMath::Lerp(1.f, PeakSpeedOvershootMultiplier, Alpha);
}

float FMidanMistakeModel::GetLateralOffsetCm() const
{
	if (!IsActive())
	{
		return 0.f;
	}
	const float Alpha = RemainingSeconds / ActiveDurationSeconds;
	return FMath::Lerp(0.f, PeakLateralOffsetCm, Alpha);
}
