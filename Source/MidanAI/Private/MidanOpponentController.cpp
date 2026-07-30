#include "MidanOpponentController.h"

#include "EngineUtils.h"
#include "Engine/AssetManager.h"
#include "MidanAIDifficultyDataAsset.h"
#include "MidanAvoidanceComponent.h"
#include "MidanLogChannels.h"
#include "MidanMathUtils.h"
#include "MidanOvertakeComponent.h"
#include "MidanRacingLineFollower.h"
#include "MidanRacingLineSpline.h"
#include "MidanRaceStateInterface.h"
#include "MidanRubberBandComponent.h"
#include "MidanServiceLocator.h"
#include "MidanVehicleInterface.h"

AMidanOpponentController::AMidanOpponentController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = true;

	Overtake = CreateDefaultSubobject<UMidanOvertakeComponent>(TEXT("Overtake"));
	Avoidance = CreateDefaultSubobject<UMidanAvoidanceComponent>(TEXT("Avoidance"));
	RubberBand = CreateDefaultSubobject<UMidanRubberBandComponent>(TEXT("RubberBand"));
}

void AMidanOpponentController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	for (TActorIterator<AMidanRacingLineSpline> It(GetWorld()); It; ++It)
	{
		RacingLine = *It;
		break;
	}

	if (!RacingLine)
	{
		UE_LOG(LogMidanAI, Error, TEXT("AMidanOpponentController '%s': no AMidanRacingLineSpline found in the level. This opponent will not drive."), *GetName());
	}

	if (Difficulty.IsNull())
	{
		UE_LOG(LogMidanAI, Error, TEXT("AMidanOpponentController '%s': no Difficulty asset set. This opponent will not drive."), *GetName());
		return;
	}

	DifficultyLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		Difficulty.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &AMidanOpponentController::OnDifficultyLoaded));
}

void AMidanOpponentController::OnUnPossess()
{
	Super::OnUnPossess();

	LoadedDifficulty = nullptr;
	Follower = nullptr;
	ThrottlePID.Reset();
	BrakePID.Reset();
}

void AMidanOpponentController::OnDifficultyLoaded()
{
	LoadedDifficulty = Difficulty.Get();
	if (!LoadedDifficulty || !RacingLine)
	{
		return;
	}

	Follower = NewObject<UMidanRacingLineFollower>(this);
	Follower->Initialise(RacingLine, LoadedDifficulty);

	Overtake->Initialise(LoadedDifficulty);
	Avoidance->Initialise(LoadedDifficulty);
	RubberBand->Initialise(LoadedDifficulty);

	ThrottlePID = FMidanPIDController();
	ThrottlePID.ProportionalGain = LoadedDifficulty->ThrottleProportionalGain;
	ThrottlePID.IntegralGain = LoadedDifficulty->ThrottleIntegralGain;
	ThrottlePID.DerivativeGain = LoadedDifficulty->ThrottleDerivativeGain;
	ThrottlePID.IntegralClamp = LoadedDifficulty->ThrottleIntegralClamp;

	BrakePID = FMidanPIDController();
	BrakePID.ProportionalGain = LoadedDifficulty->BrakeProportionalGain;
	BrakePID.IntegralGain = LoadedDifficulty->BrakeIntegralGain;
	BrakePID.DerivativeGain = LoadedDifficulty->BrakeDerivativeGain;
	BrakePID.IntegralClamp = LoadedDifficulty->BrakeIntegralClamp;

	// Deterministic per-instance seed: same racer, same seed, same mistakes,
	// which is what makes a specific AI's behaviour reproducible while tuning.
	MistakeModel.Seed(static_cast<int32>(GetUniqueID()));
}

void AMidanOpponentController::PushDelayedInputSample(float TimestampSeconds, const FMidanVehicleInputState& Input)
{
	ReactionDelayBuffer[ReactionDelayWriteIndex] = { TimestampSeconds, Input };
	ReactionDelayWriteIndex = (ReactionDelayWriteIndex + 1) % ReactionDelayBufferCapacity;
	ReactionDelaySampleCount = FMath::Min(ReactionDelaySampleCount + 1, ReactionDelayBufferCapacity);
}

FMidanVehicleInputState AMidanOpponentController::PopDelayedInput(float NowSeconds, float DelaySeconds) const
{
	FMidanVehicleInputState Result; // neutral until the buffer has a sample old enough

	const int32 OldestIndex = (ReactionDelayWriteIndex - ReactionDelaySampleCount + ReactionDelayBufferCapacity) % ReactionDelayBufferCapacity;
	const float CutoffSeconds = NowSeconds - DelaySeconds;

	// Samples are stored oldest-to-newest; the delayed input is the newest
	// one that is still old enough, so keep taking samples until one is too
	// recent, then stop.
	for (int32 i = 0; i < ReactionDelaySampleCount; ++i)
	{
		const int32 Index = (OldestIndex + i) % ReactionDelayBufferCapacity;
		if (ReactionDelayBuffer[Index].TimestampSeconds > CutoffSeconds)
		{
			break;
		}
		Result = ReactionDelayBuffer[Index].Input;
	}

	return Result;
}

FMidanVehicleInputState AMidanOpponentController::ComputeControlInput(float DeltaSeconds, APawn* Pawn, IMidanVehicleInterface* Vehicle)
{
	FMidanVehicleFrameState FrameState;
	Vehicle->GetVehicleFrameState(FrameState);

	const FVector Location = Pawn->GetActorLocation();
	const FVector Forward = Pawn->GetActorForwardVector();
	const float SpeedKmh = Vehicle->GetForwardSpeedKmh();

	const float ArcLength = Follower->GetArcLengthAtLocation(Location);
	const FRacingLineSample Sample = RacingLine->GetSampleAtDistance(ArcLength);

	// Mistake model: edge-triggered on entering a braking zone, ticked every
	// update regardless so an active mistake decays on schedule.
	if (Sample.bBrakingZone && !bWasInBrakingZone)
	{
		MistakeModel.OnEnteredBrakingZone(
			LoadedDifficulty->MistakeProbability,
			LoadedDifficulty->MistakeSpeedOvershootMultiplier,
			LoadedDifficulty->MistakeLateralOffsetCm,
			LoadedDifficulty->MistakeDurationSeconds);
	}
	bWasInBrakingZone = Sample.bBrakingZone;
	MistakeModel.Tick(DeltaSeconds);

	RubberBand->UpdateEnvelope(DeltaSeconds);

	// Target speed: uniform TargetSpeedMultiplier everywhere, plus
	// sqrt(TyreFrictionMultiplier) specifically where the reference profile
	// is braking-limited rather than merely capped — see
	// UMidanAIDifficultyDataAsset::TyreFrictionMultiplier's comment for why
	// sqrt.
	float TargetSpeedKmh = Sample.ReferenceTargetSpeedKmh * LoadedDifficulty->TargetSpeedMultiplier;
	if (Sample.bBrakingZone)
	{
		TargetSpeedKmh *= FMath::Sqrt(FMath::Max(LoadedDifficulty->TyreFrictionMultiplier, KINDA_SMALL_NUMBER));
	}
	TargetSpeedKmh *= RubberBand->GetCurrentMultiplier();
	TargetSpeedKmh *= MistakeModel.GetSpeedOvershootMultiplier();

	// Lateral offset: avoidance overrides overtaking — not crashing outranks
	// completing a pass — then the mistake model's wide-line perturbation
	// adds on top, all clamped to what the racing line says is safe here.
	const float AvoidanceOffsetCm = Avoidance->GetAvoidanceOffsetCm();
	const float OvertakeOffsetCm = Overtake->GetOvertakeOffsetCm();
	float LateralOffsetCm = FMath::IsNearlyZero(AvoidanceOffsetCm) ? OvertakeOffsetCm : AvoidanceOffsetCm;
	LateralOffsetCm += MistakeModel.GetLateralOffsetCm();
	LateralOffsetCm = FMath::Clamp(LateralOffsetCm, Sample.LateralOffsetMinCm, Sample.LateralOffsetMaxCm);

	const float DesiredSteer = Follower->ComputeSteering(Location, Forward, SpeedKmh, LateralOffsetCm);
	LastSteerNormalised = MidanMath::RateLimitedApproach(
		LastSteerNormalised, DesiredSteer, LoadedDifficulty->SteeringRateLimitPerSecond, LoadedDifficulty->SteeringRateLimitPerSecond, DeltaSeconds);

	FMidanVehicleInputState Input;
	Input.Steer = LastSteerNormalised;

	const float EmergencyBrakeFactor = Avoidance->GetEmergencyBrakeFactor();
	const float SpeedErrorKmh = TargetSpeedKmh - SpeedKmh;

	if (SpeedErrorKmh >= 0.f && EmergencyBrakeFactor <= 0.f)
	{
		Input.Throttle = FMath::Clamp(ThrottlePID.Update(SpeedErrorKmh, DeltaSeconds), 0.f, 1.f);
		Input.Brake = 0.f;
		BrakePID.Reset();
	}
	else
	{
		const float SpeedBrake = (SpeedErrorKmh < 0.f) ? BrakePID.Update(-SpeedErrorKmh, DeltaSeconds) : 0.f;
		Input.Brake = FMath::Clamp(FMath::Max(SpeedBrake, EmergencyBrakeFactor), 0.f, 1.f);
		Input.Throttle = 0.f;
		ThrottlePID.Reset();
	}

	Input.Sanitise();
	return Input;
}

void AMidanOpponentController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APawn* Pawn = GetPawn();
	IMidanVehicleInterface* Vehicle = Pawn ? Cast<IMidanVehicleInterface>(Pawn) : nullptr;
	if (!Vehicle || !LoadedDifficulty || !RacingLine || !Follower)
	{
		return;
	}

	UMidanServiceLocatorSubsystem* Locator = GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>();
	IMidanRaceStateInterface* RaceState = Locator ? Locator->GetRaceState().GetInterface() : nullptr;

	// Hard rule: never drive during the countdown, and coast once the race
	// has finished — docs/ASSUMPTIONS.md A9. IsRacingActive() covers both:
	// it is false outside Race.State.Racing.
	const bool bMayDrive = RaceState && RaceState->IsRacingActive() && !Vehicle->IsInputLocked();

	const FMidanVehicleInputState ComputedInput = bMayDrive
		? ComputeControlInput(DeltaSeconds, Pawn, Vehicle)
		: FMidanVehicleInputState();

	const float Now = GetWorld()->GetTimeSeconds();
	PushDelayedInputSample(Now, ComputedInput);

	FMidanVehicleInputState DelayedInput = PopDelayedInput(Now, LoadedDifficulty->ReactionDelaySeconds);
	DelayedInput.Sanitise();

	Vehicle->ApplyInput(DelayedInput);
}
