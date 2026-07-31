// Functional test: a vehicle spawns, its setup asset applies, and it
// accelerates under a simple throttle input.
//
// Responsibility: prove the Phase 3 vehicle-spawn path works end to end in a
// real map, not just compiles.
// Single reason to change: what "the vehicle spawned correctly" means changes.
//
// docs/ARCHITECTURE.md §5's test-surface table lists this as "Vehicle spawns
// and drives — Functional Test — map-based." AMidanVehiclePawn's setup load
// is async (CLAUDE.md forbids synchronous loads), so this test's real job is
// confirming that async chain actually completes and unlocks driving within
// a reasonable time — the failure mode a unit test cannot see.

#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "VehicleSpawnFunctionalTest.generated.h"

UCLASS()
class MIDANTESTS_API AVehicleSpawnFunctionalTest : public AFunctionalTest
{
	GENERATED_BODY()

public:
	AVehicleSpawnFunctionalTest();

	/** Vehicle class to spawn — set on the test's placed instance in the
	 *  functional test map, per class, per run. */
	UPROPERTY(EditInstanceOnly, Category = "Midan|Test")
	TSubclassOf<APawn> VehicleClass;

	/** Seconds allowed for the async setup load to complete before this test
	 *  fails rather than hangs. */
	UPROPERTY(EditInstanceOnly, Category = "Midan|Test", meta = (ClampMin = "1.0"))
	float SetupTimeoutSeconds = 10.f;

	/** Minimum forward speed the vehicle must reach under sustained full
	 *  throttle for the test to pass — proves ApplyInput actually drives the
	 *  wheels, not just that the pawn exists. */
	UPROPERTY(EditInstanceOnly, Category = "Midan|Test", meta = (ClampMin = "1.0"))
	float MinimumSpeedKmh = 20.f;

	//~ AFunctionalTest
	virtual void StartTest() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<APawn> SpawnedVehicle;

	float ElapsedSeconds = 0.f;
};
