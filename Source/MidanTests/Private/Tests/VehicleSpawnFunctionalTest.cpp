#include "Tests/VehicleSpawnFunctionalTest.h"

#include "MidanVehicleInterface.h"

AVehicleSpawnFunctionalTest::AVehicleSpawnFunctionalTest()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AVehicleSpawnFunctionalTest::StartTest()
{
	Super::StartTest();

	ElapsedSeconds = 0.f;

	if (!VehicleClass)
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("VehicleClass is unset on this test instance."));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnedVehicle = GetWorld()->SpawnActor<APawn>(VehicleClass, GetActorTransform(), SpawnParams);

	if (!SpawnedVehicle || !SpawnedVehicle->Implements<UMidanVehicleInterface>())
	{
		FinishTest(EFunctionalTestResult::Failed, TEXT("Spawned actor is null or does not implement IMidanVehicleInterface."));
		return;
	}

	// Unlock manually — this test is not run through AMidanRaceGameMode's
	// grid flow, so nothing else will unlock it.
	Cast<IMidanVehicleInterface>(SpawnedVehicle)->SetInputLocked(false);
}

void AVehicleSpawnFunctionalTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!SpawnedVehicle)
	{
		return;
	}

	ElapsedSeconds += DeltaSeconds;

	IMidanVehicleInterface* Vehicle = Cast<IMidanVehicleInterface>(SpawnedVehicle);

	FMidanVehicleInputState Input;
	Input.Throttle = 1.f;
	Vehicle->ApplyInput(Input);

	const float SpeedKmh = FMath::Abs(Vehicle->GetForwardSpeedKmh());

	if (SpeedKmh >= MinimumSpeedKmh)
	{
		FinishTest(EFunctionalTestResult::Succeeded, FString::Printf(TEXT("Reached %.1fkm/h after %.1fs."), SpeedKmh, ElapsedSeconds));
		return;
	}

	if (ElapsedSeconds >= SetupTimeoutSeconds)
	{
		FinishTest(EFunctionalTestResult::Failed, FString::Printf(
			TEXT("Did not reach %.1fkm/h within %.1fs (reached %.1fkm/h). Setup asset may not have applied — check IsSetupApplied()."),
			MinimumSpeedKmh, SetupTimeoutSeconds, SpeedKmh));
	}
}
