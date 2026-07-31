#include "MidanHotLapReplay.h"

#include "Components/PrimitiveComponent.h"
#include "MidanGhostPlayer.h"
#include "MidanLogChannels.h"
#include "MidanServiceLocator.h"
#include "MidanTrackInterface.h"
#include "MidanVehicleInterface.h"
#include "TimerManager.h"

AMidanHotLapReplay::AMidanHotLapReplay()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AMidanHotLapReplay::StartReplay()
{
	if (bReplaying)
	{
		UE_LOG(LogMidanTelemetry, Warning, TEXT("AMidanHotLapReplay '%s': StartReplay called while already replaying."), *GetName());
		return;
	}

	APawn* Vehicle = TargetVehicle.Get();
	if (!Vehicle)
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("AMidanHotLapReplay '%s': TargetVehicle is not resolved (soft pointer unset or not loaded)."), *GetName());
		return;
	}

	GhostPlayerComponent = Vehicle->FindComponentByClass<UMidanGhostPlayer>();
	if (!GhostPlayerComponent)
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("AMidanHotLapReplay '%s': TargetVehicle has no UMidanGhostPlayer component."), *GetName());
		return;
	}

	if (!GhostPlayerComponent->LoadFromFile(GhostFilePath))
	{
		UE_LOG(LogMidanTelemetry, Error, TEXT("AMidanHotLapReplay '%s': failed to load ghost file '%s'."), *GetName(), *GhostFilePath);
		return;
	}

	if (IMidanVehicleInterface* VehicleInterface = Cast<IMidanVehicleInterface>(Vehicle))
	{
		VehicleInterface->SetInputLocked(false);
	}

	CurrentRunIndex = 0;
	bReplaying = true;

	BeginNextRun();
}

void AMidanHotLapReplay::BeginNextRun()
{
	if (CurrentRunIndex >= RunCount)
	{
		bReplaying = false;
		UE_LOG(LogMidanTelemetry, Log, TEXT("AMidanHotLapReplay '%s': all %d run(s) complete."), *GetName(), RunCount);
		OnReplayFinished.Broadcast();
		return;
	}

	ResetVehicleToStartLine();

	GhostPlayerComponent->StartPlayback();
	OnRunStarted.Broadcast(CurrentRunIndex);

	GetWorld()->GetTimerManager().SetTimer(PollTimerHandle, this, &AMidanHotLapReplay::PollForRunCompletion, PollIntervalSeconds, true);
}

void AMidanHotLapReplay::PollForRunCompletion()
{
	if (!GhostPlayerComponent || !GhostPlayerComponent->HasFinished())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(PollTimerHandle);

	OnRunCompleted.Broadcast(CurrentRunIndex);
	++CurrentRunIndex;

	BeginNextRun();
}

void AMidanHotLapReplay::ResetVehicleToStartLine()
{
	APawn* Vehicle = TargetVehicle.Get();
	if (!Vehicle)
	{
		return;
	}

	// Every run starts from the identical transform and zero velocity, or
	// run 1 (launching from wherever the previous run ended) and runs 2-3
	// (launching from the finish line) would not be comparable —
	// docs/PERFORMANCE_BUDGET.md §4's whole point.
	UMidanServiceLocatorSubsystem* Locator = GetWorld() ? GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>() : nullptr;
	IMidanTrackInterface* Track = Locator ? Locator->GetTrack().GetInterface() : nullptr;
	if (!Track)
	{
		UE_LOG(LogMidanTelemetry, Warning, TEXT("AMidanHotLapReplay '%s': no track registered — cannot reset to the start line between runs."), *GetName());
		return;
	}

	const FTransform StartTransform = Track->GetTransformAtDistance(0.f);
	Vehicle->SetActorTransform(StartTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Vehicle->GetRootComponent()))
	{
		RootPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
		RootPrimitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}

void AMidanHotLapReplay::StopReplay()
{
	bReplaying = false;
	GetWorld()->GetTimerManager().ClearTimer(PollTimerHandle);

	if (GhostPlayerComponent)
	{
		GhostPlayerComponent->StopPlayback();
	}
}
