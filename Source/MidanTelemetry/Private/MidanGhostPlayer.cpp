#include "MidanGhostPlayer.h"

#include "Components/PrimitiveComponent.h"
#include "MidanLogChannels.h"
#include "MidanVehicleInterface.h"

UMidanGhostPlayer::UMidanGhostPlayer()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMidanGhostPlayer::BeginPlay()
{
	Super::BeginPlay();

	// API VERIFY — docs/ASSUMPTIONS.md A27, same as every async physics
	// component in this project.
	SetAsyncPhysicsTickEnabled(true);
}

bool UMidanGhostPlayer::LoadFromFile(const FString& FilePath)
{
	bLoaded = MidanGhostIO::LoadRecording(FilePath, Recording);
	return bLoaded;
}

void UMidanGhostPlayer::StartPlayback()
{
	if (!bLoaded)
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("UMidanGhostPlayer on '%s': StartPlayback called with no recording loaded."), *GetNameSafe(GetOwner()));
		return;
	}

	ElapsedPlaybackSeconds = 0.f;
	CurrentInputIndex = 0;
	NextResyncKeyIndex = 0;
	bPlaying = true;
}

void UMidanGhostPlayer::StopPlayback()
{
	bPlaying = false;
}

bool UMidanGhostPlayer::HasFinished() const
{
	if (Recording.InputFrames.Num() == 0)
	{
		return true;
	}
	return ElapsedPlaybackSeconds >= Recording.InputFrames.Last().TimeSeconds;
}

int32 UMidanGhostPlayer::AdvanceToInputIndexForTime(float TimeSeconds)
{
	const int32 Count = Recording.InputFrames.Num();
	while (CurrentInputIndex + 1 < Count && Recording.InputFrames[CurrentInputIndex + 1].TimeSeconds <= TimeSeconds)
	{
		++CurrentInputIndex;
	}
	return CurrentInputIndex;
}

void UMidanGhostPlayer::ApplyResyncIfDue(float TimeSeconds)
{
	if (NextResyncKeyIndex >= Recording.ResyncKeys.Num())
	{
		return;
	}

	const FMidanGhostResyncKey& Key = Recording.ResyncKeys[NextResyncKeyIndex];
	if (Key.TimeSeconds > TimeSeconds)
	{
		return;
	}
	++NextResyncKeyIndex;

	AActor* Owner = GetOwner();
	IMidanVehicleInterface* Vehicle = Owner ? Cast<IMidanVehicleInterface>(Owner) : nullptr;
	if (!Owner || !Vehicle)
	{
		return;
	}

	FMidanVehicleFrameState CurrentState;
	Vehicle->GetVehicleFrameState(CurrentState);

	const float PositionDriftCm = FVector::Dist(CurrentState.Transform.GetLocation(), Key.Transform.GetLocation());
	const float VelocityDriftCmS = FVector::Dist(CurrentState.LinearVelocity, Key.LinearVelocity);

	if (PositionDriftCm <= ResyncPositionToleranceCm && VelocityDriftCmS <= ResyncVelocityToleranceCmS)
	{
		return; // within tolerance — input replay alone is tracking correctly
	}

	UE_LOG(LogMidanTelemetry, Warning,
		TEXT("UMidanGhostPlayer on '%s': resync at t=%.1fs — position drift %.1fcm, velocity drift %.1fcm/s. Snapping."),
		*GetNameSafe(Owner), Key.TimeSeconds, PositionDriftCm, VelocityDriftCmS);

	Owner->SetActorTransform(Key.Transform, false, nullptr, ETeleportType::TeleportPhysics);
	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		RootPrimitive->SetPhysicsLinearVelocity(Key.LinearVelocity);
	}
}

void UMidanGhostPlayer::AsyncPhysicsTickComponent(float DeltaTime, float SimTime)
{
	if (!bPlaying || !bLoaded)
	{
		return;
	}

	IMidanVehicleInterface* Vehicle = Cast<IMidanVehicleInterface>(GetOwner());
	if (!Vehicle)
	{
		return;
	}

	ElapsedPlaybackSeconds += DeltaTime;

	if (HasFinished())
	{
		bPlaying = false;
		return;
	}

	const int32 Index = AdvanceToInputIndexForTime(ElapsedPlaybackSeconds);
	Vehicle->ApplyInput(Recording.InputFrames[Index].Input);

	ApplyResyncIfDue(ElapsedPlaybackSeconds);
}
