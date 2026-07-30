#include "MidanAvoidanceComponent.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "MidanAIDifficultyDataAsset.h"
#include "MidanVehicleInterface.h"
#include "TimerManager.h"

UMidanAvoidanceComponent::UMidanAvoidanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMidanAvoidanceComponent::Initialise(const UMidanAIDifficultyDataAsset* Difficulty)
{
	if (Difficulty)
	{
		TraceIntervalSeconds = Difficulty->AvoidanceTraceIntervalSeconds;
		MaxOffsetCm = Difficulty->AvoidanceMaxOffsetCm;
	}
}

void UMidanAvoidanceComponent::BeginPlay()
{
	Super::BeginPlay();

	// Stagger: a random phase offset on the first fire only. After that every
	// instance ticks at the same interval, but their phases stay spread
	// because nothing ever resynchronises them.
	const float FirstDelaySeconds = FMath::FRandRange(0.f, FMath::Max(TraceIntervalSeconds, 0.01f));
	GetWorld()->GetTimerManager().SetTimer(TraceTimerHandle, this, &UMidanAvoidanceComponent::PerformTrace, TraceIntervalSeconds, true, FirstDelaySeconds);
}

void UMidanAvoidanceComponent::PerformTrace()
{
	AAIController* AIController = Cast<AAIController>(GetOwner());
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	IMidanVehicleInterface* Vehicle = Pawn ? Cast<IMidanVehicleInterface>(Pawn) : nullptr;

	if (!Pawn || !Vehicle)
	{
		CurrentOffsetCm = 0.f;
		CurrentEmergencyBrakeFactor = 0.f;
		return;
	}

	FMidanVehicleFrameState FrameState;
	Vehicle->GetVehicleFrameState(FrameState);

	const FVector Location = Pawn->GetActorLocation();
	const FVector Right = Pawn->GetActorRightVector();
	const float SpeedCmS = FrameState.LinearVelocity.Size();
	const FVector TravelDirection = (SpeedCmS > KINDA_SMALL_NUMBER) ? FrameState.LinearVelocity.GetSafeNormal() : Pawn->GetActorForwardVector();

	const float ProjectedDistanceCm = SpeedCmS * PredictionHorizonSeconds;
	if (ProjectedDistanceCm < TraceSphereRadiusCm)
	{
		// Near standstill: nothing meaningful to predict.
		CurrentOffsetCm = 0.f;
		CurrentEmergencyBrakeFactor = 0.f;
		return;
	}

	const FVector EndLocation = Location + TravelDirection * ProjectedDistanceCm;
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(TraceSphereRadiusCm);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MidanAIAvoidance), false, Pawn);

	FHitResult ForwardHit;
	// API VERIFY: ECC_Pawn is assumed to be the channel both vehicle bodies
	// and static track geometry block on. Confirm against the project's
	// collision profile setup (see VehicleSetupApplier's chassis/wheel notes)
	// once the editor is available.
	const bool bForwardHit = GetWorld()->SweepSingleByChannel(ForwardHit, Location, EndLocation, FQuat::Identity, ECC_Pawn, Sphere, QueryParams);

	if (!bForwardHit)
	{
		CurrentOffsetCm = 0.f;
		CurrentEmergencyBrakeFactor = 0.f;
		return;
	}

	FHitResult LeftHit;
	FHitResult RightHit;
	const bool bLeftBlocked = GetWorld()->SweepSingleByChannel(
		LeftHit, Location - Right * SideProbeOffsetCm, EndLocation - Right * SideProbeOffsetCm, FQuat::Identity, ECC_Pawn, Sphere, QueryParams);
	const bool bRightBlocked = GetWorld()->SweepSingleByChannel(
		RightHit, Location + Right * SideProbeOffsetCm, EndLocation + Right * SideProbeOffsetCm, FQuat::Identity, ECC_Pawn, Sphere, QueryParams);

	float TargetOffsetCm = 0.f;
	if (bLeftBlocked && !bRightBlocked)
	{
		TargetOffsetCm = MaxOffsetCm; // dodge right
	}
	else if (bRightBlocked && !bLeftBlocked)
	{
		TargetOffsetCm = -MaxOffsetCm; // dodge left
	}
	else if (!bLeftBlocked && !bRightBlocked)
	{
		// Both sides clear: dodge away from the hit surface's lateral component.
		const float NormalRightDot = FVector::DotProduct(ForwardHit.ImpactNormal, Right);
		TargetOffsetCm = (NormalRightDot >= 0.f ? -1.f : 1.f) * MaxOffsetCm;
	}
	// Both sides blocked: TargetOffsetCm stays 0 — no lateral escape, brake instead.

	CurrentOffsetCm = TargetOffsetCm;

	const float TimeToImpactSeconds = FVector::Dist(Location, ForwardHit.ImpactPoint) / FMath::Max(SpeedCmS, 1.f);
	const float Urgency = 1.f - FMath::Clamp(TimeToImpactSeconds / PredictionHorizonSeconds, 0.f, 1.f);

	// A dodge in progress needs less braking help than one with no lateral
	// escape at all — per the class comment, avoidance prefers steering.
	CurrentEmergencyBrakeFactor = FMath::IsNearlyZero(TargetOffsetCm) ? Urgency : (Urgency * 0.3f);
}
