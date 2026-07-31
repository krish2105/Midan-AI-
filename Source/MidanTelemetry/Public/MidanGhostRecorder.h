// Records FMidanVehicleInputState per physics step + periodic
// FMidanVehicleFrameState keyframes.
//
// Responsibility: turn a live drive into a saveable FMidanGhostRecording.
// Single reason to change: what gets recorded, or how often, changes.
//
// Attach to any actor implementing IMidanVehicleInterface — this component
// lives in MidanTelemetry (MidanCore dependency only) and never names
// AMidanVehiclePawn, the same decoupling every telemetry and race system in
// this project uses. Samples in the ASYNC PHYSICS CALLBACK, matching
// UVehicleAeroComponent's cadence, because a ghost must be recorded at the
// same rate the physics that produced it ran, not at frame rate — recording
// at frame rate would make ghost fidelity depend on the recording machine's
// performance.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MidanGhostData.h"
#include "MidanGhostRecorder.generated.h"

UCLASS(ClassGroup = (Midan), meta = (BlueprintSpawnableComponent))
class MIDANTELEMETRY_API UMidanGhostRecorder : public UActorComponent
{
	GENERATED_BODY()

public:
	UMidanGhostRecorder();

	/** How often a full-state resync key is captured, seconds. Coarser than
	 *  the input sample rate by design — resync keys exist only to bound
	 *  drift, not to reconstruct motion, so they do not need physics-rate
	 *  density. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|Ghost", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float ResyncKeyIntervalSeconds = 5.f;

	UFUNCTION(BlueprintCallable, Category = "Midan|Ghost")
	void StartRecording();

	UFUNCTION(BlueprintCallable, Category = "Midan|Ghost")
	void StopRecording();

	UFUNCTION(BlueprintPure, Category = "Midan|Ghost")
	bool IsRecording() const { return bRecording; }

	/** Writes the completed recording to disk via MidanGhostIO::SaveRecording.
	 *  Call after StopRecording. Same "one-shot, explicitly triggered"
	 *  exception to the game-thread-I/O rule as
	 *  UMidanTelemetrySubsystem::ExportCaptureToCsv — this is not part of
	 *  the continuous per-substep recording path. */
	UFUNCTION(BlueprintCallable, Category = "Midan|Ghost")
	bool SaveToFile(const FString& FilePath) const;

	//~ UActorComponent
	virtual void BeginPlay() override;
	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;

private:
	bool bRecording = false;
	float ElapsedRecordingSeconds = 0.f;
	float TimeSinceLastResyncKey = 0.f;

	FMidanGhostRecording Recording;
};
