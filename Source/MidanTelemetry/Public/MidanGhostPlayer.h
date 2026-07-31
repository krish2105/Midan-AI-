// Replays INPUTS through ApplyInput with resync correction.
//
// Responsibility: drive a vehicle from a recorded FMidanGhostRecording.
// Single reason to change: how playback advances or resyncs changes.
//
// Replays through IMidanVehicleInterface::ApplyInput — the identical path
// the player and every AI opponent use (docs/ARCHITECTURE.md §3.5). This is
// what makes a ghost a genuine demonstration of deterministic physics rather
// than a cheap transform-scrubbing trick: the same inputs, replayed through
// the same door, should reproduce (closely enough to need only occasional
// correction) the same motion.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MidanGhostData.h"
#include "MidanGhostPlayer.generated.h"

UCLASS(ClassGroup = (Midan), meta = (BlueprintSpawnableComponent))
class MIDANTELEMETRY_API UMidanGhostPlayer : public UActorComponent
{
	GENERATED_BODY()

public:
	UMidanGhostPlayer();

	/** Drift beyond this at a resync key snaps the transform back to the
	 *  recorded one. Loose enough that ordinary substep-to-substep floating
	 *  point variance never triggers a visible correction; tight enough that
	 *  a genuinely diverged replay (e.g. after a physics-affecting content
	 *  change) is caught rather than silently drifting further every lap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|Ghost", meta = (ClampMin = "5.0"))
	float ResyncPositionToleranceCm = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|Ghost", meta = (ClampMin = "5.0"))
	float ResyncVelocityToleranceCmS = 100.f;

	/** Synchronous load — same one-shot-explicit-action exception as
	 *  UMidanGhostRecorder::SaveToFile. Call before StartPlayback. */
	UFUNCTION(BlueprintCallable, Category = "Midan|Ghost")
	bool LoadFromFile(const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "Midan|Ghost")
	void StartPlayback();

	UFUNCTION(BlueprintCallable, Category = "Midan|Ghost")
	void StopPlayback();

	UFUNCTION(BlueprintPure, Category = "Midan|Ghost")
	bool IsPlaying() const { return bPlaying; }

	/** True once ElapsedPlaybackSeconds has passed the last recorded input
	 *  sample. The caller (e.g. MidanHotLapReplay, Phase 8) decides what
	 *  happens next — loop, stop the capture, respawn — this component only
	 *  reports the fact. */
	UFUNCTION(BlueprintPure, Category = "Midan|Ghost")
	bool HasFinished() const;

	//~ UActorComponent
	virtual void BeginPlay() override;
	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;

private:
	int32 AdvanceToInputIndexForTime(float TimeSeconds);
	void ApplyResyncIfDue(float TimeSeconds);

	FMidanGhostRecording Recording;
	bool bLoaded = false;
	bool bPlaying = false;

	float ElapsedPlaybackSeconds = 0.f;

	/** Playback time is monotonic, so both search cursors only ever advance
	 *  forward — a linear scan from the last position, not a fresh search
	 *  every substep. */
	int32 CurrentInputIndex = 0;
	int32 NextResyncKeyIndex = 0;
};
