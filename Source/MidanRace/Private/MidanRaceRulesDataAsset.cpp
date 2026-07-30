#include "MidanRaceRulesDataAsset.h"

#define LOCTEXT_NAMESPACE "MidanRaceRulesDataAsset"

void UMidanRaceRulesDataAsset::ValidateMidanData(FMidanValidationResult& Result) const
{
	if (LapCount < 1)
	{
		Result.AddError(TEXT("LapCount"),
			LOCTEXT("BadLapCount", "Lap count must be at least 1."));
	}

	if (SectorCount < 1 || SectorCount > MidanRaceConstants::MaxSectorCount)
	{
		Result.AddError(TEXT("SectorCount"), FText::Format(
			LOCTEXT("BadSectorCountFmt", "Sector count must be between 1 and {0}."),
			FText::AsNumber(MidanRaceConstants::MaxSectorCount)));
	}

	if (OffTrackGraceSeconds <= 0.f)
	{
		Result.AddError(TEXT("OffTrackGraceSeconds"),
			LOCTEXT("BadGrace", "Off-track grace must be positive. Zero means any single graced wheel invalidates the lap instantly, which is not a grace period."));
	}

	if (OffTrackPollIntervalSeconds >= OffTrackGraceSeconds)
	{
		Result.AddError(TEXT("OffTrackPollIntervalSeconds"),
			LOCTEXT("PollTooCoarse", "Poll interval must be smaller than the grace period, or a single poll tick can invalidate a lap that never actually exceeded the grace window."));
	}
	else if (OffTrackPollIntervalSeconds > OffTrackGraceSeconds * 0.34f)
	{
		Result.AddWarning(TEXT("OffTrackPollIntervalSeconds"),
			LOCTEXT("PollCoarse", "Poll interval is more than a third of the grace period. Grace time will be quantised into visibly uneven chunks — consider polling faster."));
	}

	if (ResultsDelaySeconds < 0.f)
	{
		Result.AddError(TEXT("ResultsDelaySeconds"),
			LOCTEXT("BadResultsDelay", "Results delay cannot be negative."));
	}

	if (RespawnCooldownSeconds <= 0.f)
	{
		Result.AddError(TEXT("RespawnCooldownSeconds"),
			LOCTEXT("BadRespawnCooldown", "Respawn cooldown must be positive."));
	}

	if (PositionUpdateHz <= 0.f)
	{
		Result.AddError(TEXT("PositionUpdateHz"),
			LOCTEXT("BadPositionHz", "Position update rate must be positive."));
	}
	else if (PositionUpdateHz > 30.f)
	{
		Result.AddWarning(TEXT("PositionUpdateHz"),
			LOCTEXT("PositionHzHigh", "Position update above 30Hz. Master prompt §3.3 specifies 10Hz deliberately, not per-frame — confirm this is intentional before shipping it."));
	}
}

#undef LOCTEXT_NAMESPACE
