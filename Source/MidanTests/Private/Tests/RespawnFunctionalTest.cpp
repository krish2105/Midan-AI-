#include "Tests/RespawnFunctionalTest.h"

#include "MidanRespawnComponent.h"
#include "MidanServiceLocator.h"
#include "MidanTrackInterface.h"
#include "MidanVehicleInterface.h"

ARespawnFunctionalTest::ARespawnFunctionalTest()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ARespawnFunctionalTest::StartTest()
{
	Super::StartTest();

	ElapsedSeconds = 0.f;
	bRespawnRequested = false;

	APawn* Vehicle = TargetVehicle.Get();
	if (!Vehicle)
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("TargetVehicle did not resolve."));
		return;
	}

	// Displace it well off the racing surface.
	const FVector Displaced = Vehicle->GetActorLocation() + Vehicle->GetActorRightVector() * DisplacementDistanceCm;
	Vehicle->SetActorLocation(Displaced, false, nullptr, ETeleportType::TeleportPhysics);
}

void ARespawnFunctionalTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APawn* Vehicle = TargetVehicle.Get();
	if (!Vehicle)
	{
		return;
	}

	ElapsedSeconds += DeltaSeconds;

	if (!bRespawnRequested)
	{
		if (UMidanRespawnComponent* Respawn = Vehicle->FindComponentByClass<UMidanRespawnComponent>())
		{
			Respawn->RequestRespawn();
			bRespawnRequested = true;
		}
		else
		{
			FinishTest(EFunctionalTestResult::Error, TEXT("TargetVehicle has no UMidanRespawnComponent."));
		}
		return;
	}

	UMidanServiceLocatorSubsystem* Locator = GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>();
	IMidanVehicleInterface* VehicleInterface = Cast<IMidanVehicleInterface>(Vehicle);
	IMidanTrackInterface* Track = Locator ? Locator->GetTrack().GetInterface() : nullptr;

	if (VehicleInterface && Track)
	{
		FMidanVehicleFrameState FrameState;
		VehicleInterface->GetVehicleFrameState(FrameState);

		if (FrameState.GetOffTrackWheelCount() == 0)
		{
			float LateralOffset = 0.f;
			Track->GetClosestDistanceToWorldLocation(Vehicle->GetActorLocation(), LateralOffset);

			if (FMath::Abs(LateralOffset) < Track->GetTrackHalfWidthAtDistance(0.f))
			{
				FinishTest(EFunctionalTestResult::Succeeded, FString::Printf(TEXT("Respawned on-track after %.1fs, lateral offset %.0fcm."), ElapsedSeconds, LateralOffset));
				return;
			}
		}
	}

	if (ElapsedSeconds >= TimeoutSeconds)
	{
		FinishTest(EFunctionalTestResult::Failed, FString::Printf(TEXT("Vehicle was not back on-track within %.1fs of the respawn request."), TimeoutSeconds));
	}
}
