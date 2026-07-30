// Composes the racing-line follower and longitudinal PID into the AI driver.
// Outputs ONLY a normalised FMidanVehicleInputState through
// IMidanVehicleInterface::ApplyInput — the identical path the player uses.
//
// Responsibility: turn "where is the racing line, what does the target speed
// profile say, is there a hazard or a pass opportunity" into one input state
// per control update.
// Single reason to change: how those signals are composed into input changes.
//
// HARD RULE, restated because it is the one line in this file that must never
// move: there is no second door into the vehicle. This class never touches
// the movement component, never sets a velocity, never teleports — it calls
// ApplyInput and nothing else, exactly like UVehicleInputComponent does for
// the player. See MidanVehicleInterface.h's class comment.
//
// No Behaviour Tree (docs/ARCHITECTURE.md §3.4): racing is a continuous
// control problem — a PID loop and a pure-pursuit follower — not a sequence
// of discrete decisions, so a BT would be the wrong tool wearing the right
// hat.
//
// Ticks per-frame, staggered across the seven opponents purely by virtue of
// UMidanAvoidanceComponent and UMidanOvertakeComponent randomising their own
// first-fire time — see docs/ARCHITECTURE.md §2.3.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MidanCoreTypes.h"
#include "MidanMistakeModel.h"
#include "MidanPIDController.h"
#include "MidanOpponentController.generated.h"

class AMidanRacingLineSpline;
class UMidanAIDifficultyDataAsset;
class UMidanAvoidanceComponent;
class UMidanOvertakeComponent;
class UMidanRacingLineFollower;
class UMidanRubberBandComponent;
class IMidanVehicleInterface;

UCLASS()
class MIDANAI_API AMidanOpponentController : public AAIController
{
	GENERATED_BODY()

public:
	AMidanOpponentController();

	/** This racer's difficulty tier. Soft, async-loaded — set on the
	 *  Blueprint subclass or by AMidanRaceGameMode at spawn time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|AI")
	TSoftObjectPtr<UMidanAIDifficultyDataAsset> Difficulty;

	//~ AAIController
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void OnDifficultyLoaded();

	/** Composes every signal into one input state for this control update.
	 *  Never called while the race is not IsRacingActive() or the vehicle's
	 *  own input lock is set — Tick() gates that, per docs/ASSUMPTIONS.md A9. */
	FMidanVehicleInputState ComputeControlInput(float DeltaSeconds, APawn* Pawn, IMidanVehicleInterface* Vehicle);

	/** Fixed-capacity ring buffer implementing ReactionDelaySeconds: pushes
	 *  every computed input with a timestamp, and ApplyInput always receives
	 *  the newest sample old enough to have "reached" the AI. Zero allocation
	 *  — a plain C array, not a TArray, sized generously (256 samples covers
	 *  a 1s delay up to 256fps). */
	void PushDelayedInputSample(float TimestampSeconds, const FMidanVehicleInputState& Input);
	FMidanVehicleInputState PopDelayedInput(float NowSeconds, float DelaySeconds) const;

	static constexpr int32 ReactionDelayBufferCapacity = 256;

	struct FDelayedInputSample
	{
		float TimestampSeconds = 0.f;
		FMidanVehicleInputState Input;
	};

	FDelayedInputSample ReactionDelayBuffer[ReactionDelayBufferCapacity];
	int32 ReactionDelayWriteIndex = 0;
	int32 ReactionDelaySampleCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<AMidanRacingLineSpline> RacingLine;

	UPROPERTY(Transient)
	TObjectPtr<UMidanRacingLineFollower> Follower;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Midan|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMidanOvertakeComponent> Overtake;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Midan|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMidanAvoidanceComponent> Avoidance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Midan|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMidanRubberBandComponent> RubberBand;

	UPROPERTY(Transient)
	TObjectPtr<const UMidanAIDifficultyDataAsset> LoadedDifficulty;

	TSharedPtr<struct FStreamableHandle> DifficultyLoadHandle;

	FMidanPIDController ThrottlePID;
	FMidanPIDController BrakePID;
	FMidanMistakeModel MistakeModel;

	bool bWasInBrakingZone = false;
	float LastSteerNormalised = 0.f;
};
