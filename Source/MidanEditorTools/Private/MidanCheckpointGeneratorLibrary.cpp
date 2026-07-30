#include "MidanCheckpointGeneratorLibrary.h"

#include "EngineUtils.h"
#include "MidanCheckpoint.h"
#include "MidanLogChannels.h"
#include "MidanTrackSpline.h"

bool UMidanCheckpointGeneratorLibrary::GenerateCheckpoints(AMidanTrackSpline* TrackSpline, int32 Count, int32 SectorCount, TArray<AMidanCheckpoint*>& OutCheckpoints)
{
	OutCheckpoints.Reset();

	if (!TrackSpline || !TrackSpline->GetWorld())
	{
		UE_LOG(LogMidanRace, Error, TEXT("GenerateCheckpoints: TrackSpline is null."));
		return false;
	}

	if (Count < 3)
	{
		UE_LOG(LogMidanRace, Error, TEXT("GenerateCheckpoints: Count must be at least 3 (a lap needs a finish line and at least two sequencing points). Got %d."), Count);
		return false;
	}

	if (SectorCount < 1)
	{
		UE_LOG(LogMidanRace, Error, TEXT("GenerateCheckpoints: SectorCount must be at least 1. Got %d."), SectorCount);
		return false;
	}

	UWorld* World = TrackSpline->GetWorld();

	int32 DestroyedCount = 0;
	for (TActorIterator<AMidanCheckpoint> It(World); It; ++It)
	{
		It->Destroy();
		++DestroyedCount;
	}
	if (DestroyedCount > 0)
	{
		UE_LOG(LogMidanRace, Log, TEXT("GenerateCheckpoints: destroyed %d existing checkpoint(s) before regenerating."), DestroyedCount);
	}

	const float TrackLength = TrackSpline->GetTrackLength();
	const float SpacingCm = TrackLength / static_cast<float>(Count);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float ArcLength = Index * SpacingCm;
		const FTransform CheckpointTransform = TrackSpline->GetTransformAtDistance(ArcLength);

		// Deterministic naming (docs/ARCHITECTURE.md §3.3) so a regenerated
		// track produces byte-identical actor names for the same input,
		// which keeps level diffs readable.
		SpawnParams.Name = FName(*FString::Printf(TEXT("Checkpoint_%02d"), Index));

		AMidanCheckpoint* Checkpoint = World->SpawnActor<AMidanCheckpoint>(AMidanCheckpoint::StaticClass(), CheckpointTransform, SpawnParams);
		if (!Checkpoint)
		{
			UE_LOG(LogMidanRace, Error, TEXT("GenerateCheckpoints: failed to spawn checkpoint %d."), Index);
			continue;
		}

		Checkpoint->Index = Index;
		Checkpoint->ArcLength = ArcLength;
		Checkpoint->SectorIndex = (Index * SectorCount) / Count;
		Checkpoint->HalfWidth = TrackSpline->GetTrackHalfWidthAtDistance(ArcLength);
#if WITH_EDITOR
		Checkpoint->SetActorLabel(SpawnParams.Name.ToString());
#endif

		OutCheckpoints.Add(Checkpoint);
	}

	UE_LOG(LogMidanRace, Log, TEXT("GenerateCheckpoints: generated %d checkpoints over %.0fcm (spacing %.0fcm), %d sectors."),
		OutCheckpoints.Num(), TrackLength, SpacingCm, SectorCount);

	return true;
}
