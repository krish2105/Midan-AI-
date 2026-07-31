#include "MidanGhostRecorder.h"

#include "MidanLogChannels.h"
#include "MidanVehicleInterface.h"

UMidanGhostRecorder::UMidanGhostRecorder()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMidanGhostRecorder::BeginPlay()
{
	Super::BeginPlay();

	// API VERIFY: SetAsyncPhysicsTickEnabled / AsyncPhysicsTickComponent —
	// docs/ASSUMPTIONS.md A27, same unverified-on-5.8 status as every other
	// async physics component in this project.
	SetAsyncPhysicsTickEnabled(true);
}

void UMidanGhostRecorder::StartRecording()
{
	if (bRecording)
	{
		return;
	}

	Recording = FMidanGhostRecording();
	ElapsedRecordingSeconds = 0.f;
	TimeSinceLastResyncKey = ResyncKeyIntervalSeconds; // force a key on the first substep

	bRecording = true;
}

void UMidanGhostRecorder::StopRecording()
{
	bRecording = false;
	UE_LOG(LogMidanTelemetry, Log, TEXT("UMidanGhostRecorder on '%s': stopped after %.1fs, %d input samples, %d resync keys."),
		*GetNameSafe(GetOwner()), ElapsedRecordingSeconds, Recording.InputFrames.Num(), Recording.ResyncKeys.Num());
}

bool UMidanGhostRecorder::SaveToFile(const FString& FilePath) const
{
	return MidanGhostIO::SaveRecording(FilePath, Recording);
}

void UMidanGhostRecorder::AsyncPhysicsTickComponent(float DeltaTime, float SimTime)
{
	if (!bRecording)
	{
		return;
	}

	const IMidanVehicleInterface* Vehicle = Cast<IMidanVehicleInterface>(GetOwner());
	if (!Vehicle)
	{
		return;
	}

	ElapsedRecordingSeconds += DeltaTime;

	FMidanGhostInputSample InputSample;
	InputSample.TimeSeconds = ElapsedRecordingSeconds;
	InputSample.Input = Vehicle->GetLastAppliedInput();
	Recording.InputFrames.Add(InputSample);

	TimeSinceLastResyncKey += DeltaTime;
	if (TimeSinceLastResyncKey >= ResyncKeyIntervalSeconds)
	{
		FMidanVehicleFrameState FrameState;
		Vehicle->GetVehicleFrameState(FrameState);

		FMidanGhostResyncKey Key;
		Key.TimeSeconds = ElapsedRecordingSeconds;
		Key.Transform = FrameState.Transform;
		Key.LinearVelocity = FrameState.LinearVelocity;
		Recording.ResyncKeys.Add(Key);

		TimeSinceLastResyncKey = 0.f;
	}
}
