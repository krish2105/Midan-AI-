// Functional test: respawn restores a valid, on-track state.
//
// Responsibility: prove UMidanRespawnComponent actually recovers a stuck
// vehicle in a real map, against real track geometry.
// Single reason to change: what "respawn succeeded" means changes.
//
// docs/ARCHITECTURE.md §5: "Respawn restores valid state — Functional Test —
// map-based." Deliberately displaces the vehicle far off the track first —
// the interesting failure mode is respawning INTO another hazard, which only
// shows up against real geometry.

#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "RespawnFunctionalTest.generated.h"

UCLASS()
class MIDANTESTS_API ARespawnFunctionalTest : public AFunctionalTest
{
	GENERATED_BODY()

public:
	ARespawnFunctionalTest();

	UPROPERTY(EditInstanceOnly, Category = "Midan|Test")
	TSoftObjectPtr<APawn> TargetVehicle;

	/** How far off the track centreline to displace the vehicle before
	 *  requesting a respawn, cm. Large enough to guarantee off-track state
	 *  and clear of any nearby hazard. */
	UPROPERTY(EditInstanceOnly, Category = "Midan|Test", meta = (ClampMin = "500.0"))
	float DisplacementDistanceCm = 5000.f;

	UPROPERTY(EditInstanceOnly, Category = "Midan|Test", meta = (ClampMin = "1.0"))
	float TimeoutSeconds = 15.f;

	//~ AFunctionalTest
	virtual void StartTest() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	float ElapsedSeconds = 0.f;
	bool bRespawnRequested = false;
};
