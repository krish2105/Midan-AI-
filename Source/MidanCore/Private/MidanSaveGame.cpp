#include "MidanSaveGame.h"

const FString& UMidanSaveGame::GetSaveSlotName()
{
	static const FString SlotName = TEXT("MidanRecords");
	return SlotName;
}

FMidanVehicleRecord& UMidanSaveGame::FindOrAddRecord(const FString& VehicleAssetName)
{
	for (FMidanVehicleRecord& Record : VehicleRecords)
	{
		if (Record.VehicleAssetName == VehicleAssetName)
		{
			return Record;
		}
	}

	FMidanVehicleRecord& NewRecord = VehicleRecords.AddDefaulted_GetRef();
	NewRecord.VehicleAssetName = VehicleAssetName;
	NewRecord.BestLapTimeSeconds = 0.f;
	return NewRecord;
}

const FMidanVehicleRecord* UMidanSaveGame::FindRecord(const FString& VehicleAssetName) const
{
	return VehicleRecords.FindByPredicate([&VehicleAssetName](const FMidanVehicleRecord& Record)
	{
		return Record.VehicleAssetName == VehicleAssetName;
	});
}

bool UMidanSaveGame::SubmitLapTime(const FString& VehicleAssetName, float LapTimeSeconds)
{
	FMidanVehicleRecord& Record = FindOrAddRecord(VehicleAssetName);

	// 0 means "no record yet" — BestLapTimeSeconds is never negative, so
	// a zero-or-slower check covers both the empty case and a genuine
	// improvement without a separate "has record" bool.
	if (Record.BestLapTimeSeconds > 0.f && LapTimeSeconds >= Record.BestLapTimeSeconds)
	{
		return false;
	}

	Record.BestLapTimeSeconds = LapTimeSeconds;
	return true;
}

bool UMidanSaveGame::SubmitSectorTime(const FString& VehicleAssetName, int32 SectorIndex, float SectorTimeSeconds)
{
	if (SectorIndex < 0 || SectorIndex >= MidanSaveConstants::MaxSectorCount)
	{
		return false;
	}

	FMidanVehicleRecord& Record = FindOrAddRecord(VehicleAssetName);

	if (Record.bSectorRecorded[SectorIndex] && SectorTimeSeconds >= Record.BestSectorTimeSeconds[SectorIndex])
	{
		return false;
	}

	Record.BestSectorTimeSeconds[SectorIndex] = SectorTimeSeconds;
	Record.bSectorRecorded[SectorIndex] = true;
	return true;
}
