#include "MidanPositionCalculator.h"

#include "Algo/Sort.h"

float MidanRacePosition::ComputeTotalProgress(int32 LapIndex, float ArcLength, float TrackLength)
{
	return static_cast<float>(LapIndex) * TrackLength + ArcLength;
}

TArray<int32> MidanRacePosition::ComputeRacePositions(const TArray<float>& ProgressPerRacer)
{
	const int32 Count = ProgressPerRacer.Num();

	TArray<int32> RankedIndices;
	RankedIndices.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		RankedIndices.Add(i);
	}

	// Stable sort: equal progress keeps input order, so ties resolve to "the
	// earlier index wins" rather than an unspecified sort-implementation detail.
	Algo::StableSort(RankedIndices, [&ProgressPerRacer](int32 A, int32 B)
	{
		return ProgressPerRacer[A] > ProgressPerRacer[B];
	});

	TArray<int32> Positions;
	Positions.SetNumUninitialized(Count);
	for (int32 Rank = 0; Rank < Count; ++Rank)
	{
		Positions[RankedIndices[Rank]] = Rank + 1;
	}

	return Positions;
}
