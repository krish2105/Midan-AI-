#include "MidanOvertakeComponent.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "MidanAIDifficultyDataAsset.h"
#include "MidanVehicleInterface.h"
#include "TimerManager.h"

namespace
{
	/** Trace/overlap sphere radius standing in for a car's rough footprint —
	 *  same structural reasoning as UMidanAvoidanceComponent's
	 *  TraceSphereRadiusCm. */
	constexpr float ProbeSphereRadiusCm = 120.f;
}

UMidanOvertakeComponent::UMidanOvertakeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMidanOvertakeComponent::Initialise(const UMidanAIDifficultyDataAsset* Difficulty)
{
	if (Difficulty)
	{
		CheckIntervalSeconds = Difficulty->OvertakeCheckIntervalSeconds;
		MinGapSeconds = Difficulty->OvertakeMinGapSeconds;
		DetectionRangeCm = Difficulty->OvertakeDetectionRangeCm;
		Aggression = Difficulty->Aggression;
	}
}

void UMidanOvertakeComponent::BeginPlay()
{
	Super::BeginPlay();

	const float FirstDelaySeconds = FMath::FRandRange(0.f, FMath::Max(CheckIntervalSeconds, 0.01f));
	GetWorld()->GetTimerManager().SetTimer(CheckTimerHandle, this, &UMidanOvertakeComponent::EvaluateOvertake, CheckIntervalSeconds, true, FirstDelaySeconds);
}

void UMidanOvertakeComponent::EvaluateOvertake()
{
	AAIController* AIController = Cast<AAIController>(GetOwner());
	APawn* SelfPawn = AIController ? AIController->GetPawn() : nullptr;
	IMidanVehicleInterface* SelfVehicle = SelfPawn ? Cast<IMidanVehicleInterface>(SelfPawn) : nullptr;

	if (!SelfPawn || !SelfVehicle)
	{
		CurrentOffsetCm = 0.f;
		bCommitted = false;
		TargetActor = nullptr;
		return;
	}

	FMidanVehicleFrameState SelfFrameState;
	SelfVehicle->GetVehicleFrameState(SelfFrameState);

	const FVector SelfLocation = SelfPawn->GetActorLocation();
	const FVector SelfForward = SelfPawn->GetActorForwardVector();
	const FVector SelfRight = SelfPawn->GetActorRightVector();

	// Already committed: check whether to hold or abort, and skip the target
	// search entirely — a held pass does not need a new candidate each check.
	if (bCommitted)
	{
		AActor* Target = TargetActor.Get();
		IMidanVehicleInterface* TargetVehicle = Target ? Cast<IMidanVehicleInterface>(Target) : nullptr;

		bool bShouldAbort = (TargetVehicle == nullptr);
		if (TargetVehicle)
		{
			FMidanVehicleFrameState TargetFrameState;
			TargetVehicle->GetVehicleFrameState(TargetFrameState);

			const FVector ToTarget = Target->GetActorLocation() - SelfLocation;
			const float ForwardDistance = FVector::DotProduct(ToTarget, SelfForward);
			const float ClosingSpeedCmS = FVector::DotProduct(SelfFrameState.LinearVelocity - TargetFrameState.LinearVelocity, SelfForward);
			const float GapSeconds = (ClosingSpeedCmS > KINDA_SMALL_NUMBER)
				? (ForwardDistance / ClosingSpeedCmS)
				: TNumericLimits<float>::Max();

			// Aborts once safely alongside/past (negative forward distance)
			// or once the gap has opened well past the commit threshold.
			bShouldAbort = (ForwardDistance < 0.f) || (GapSeconds > MinGapSeconds * AbortGapMultiplier);
		}

		if (bShouldAbort)
		{
			bCommitted = false;
			TargetActor = nullptr;
			CurrentOffsetCm = 0.f;
		}
		return;
	}

	// Not committed: look for a candidate to pass.
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(DetectionRangeCm);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MidanAIOvertakeScan), false, SelfPawn);

	TArray<FOverlapResult> Overlaps;
	// API VERIFY: ECC_Pawn assumed to be the channel vehicle bodies overlap
	// on — same assumption as UMidanAvoidanceComponent, confirm together.
	GetWorld()->OverlapMultiByChannel(Overlaps, SelfLocation, FQuat::Identity, ECC_Pawn, Sphere, QueryParams);

	AActor* BestCandidate = nullptr;
	float BestForwardDistance = TNumericLimits<float>::Max();
	float BestClosingSpeedCmS = 0.f;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OtherActor = Overlap.GetActor();
		if (!OtherActor || OtherActor == SelfPawn || !OtherActor->Implements<UMidanVehicleInterface>())
		{
			continue;
		}

		IMidanVehicleInterface* OtherVehicle = Cast<IMidanVehicleInterface>(OtherActor);
		FMidanVehicleFrameState OtherFrameState;
		OtherVehicle->GetVehicleFrameState(OtherFrameState);

		const FVector ToOther = OtherActor->GetActorLocation() - SelfLocation;
		const float ForwardDistance = FVector::DotProduct(ToOther, SelfForward);
		if (ForwardDistance <= 0.f || ForwardDistance >= BestForwardDistance)
		{
			continue; // behind us, or further than a candidate already found
		}

		const float ClosingSpeedCmS = FVector::DotProduct(SelfFrameState.LinearVelocity - OtherFrameState.LinearVelocity, SelfForward);
		if (ClosingSpeedCmS <= KINDA_SMALL_NUMBER)
		{
			continue; // not catching up — no point evaluating a pass
		}

		BestCandidate = OtherActor;
		BestForwardDistance = ForwardDistance;
		BestClosingSpeedCmS = ClosingSpeedCmS;
	}

	if (!BestCandidate)
	{
		return;
	}

	const float GapSeconds = BestForwardDistance / BestClosingSpeedCmS;

	// Aggression lowers the required gap: a more aggressive tier commits to
	// closer, riskier passes.
	const float EffectiveMinGapSeconds = MinGapSeconds * (1.f - FMath::Clamp(Aggression, 0.f, 1.f) * 0.5f);
	if (GapSeconds > EffectiveMinGapSeconds)
	{
		return;
	}

	// Predictive clear sweep: probe both sides over the approach distance
	// before committing to a lane.
	const FVector SweepEnd = SelfLocation + SelfForward * BestForwardDistance;
	FHitResult LeftHit;
	FHitResult RightHit;
	const bool bLeftBlocked = GetWorld()->SweepSingleByChannel(
		LeftHit, SelfLocation - SelfRight * CommittedLateralOffsetCm, SweepEnd - SelfRight * CommittedLateralOffsetCm,
		FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(ProbeSphereRadiusCm), QueryParams);
	const bool bRightBlocked = GetWorld()->SweepSingleByChannel(
		RightHit, SelfLocation + SelfRight * CommittedLateralOffsetCm, SweepEnd + SelfRight * CommittedLateralOffsetCm,
		FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(ProbeSphereRadiusCm), QueryParams);

	float ChosenOffsetCm = 0.f;
	if (!bRightBlocked)
	{
		ChosenOffsetCm = CommittedLateralOffsetCm; // convention: prefer the right lane when both are clear
	}
	else if (!bLeftBlocked)
	{
		ChosenOffsetCm = -CommittedLateralOffsetCm;
	}
	else
	{
		return; // no clear lane this check; try again next interval
	}

	bCommitted = true;
	TargetActor = BestCandidate;
	CurrentOffsetCm = ChosenOffsetCm;
}
