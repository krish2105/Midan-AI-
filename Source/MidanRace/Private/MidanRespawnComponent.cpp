#include "MidanRespawnComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/AssetManager.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "MidanLapTimingSubsystem.h"
#include "MidanLogChannels.h"
#include "MidanRaceRulesDataAsset.h"
#include "MidanServiceLocator.h"
#include "MidanTrackInterface.h"
#include "MidanVehicleInterface.h"

UMidanRespawnComponent::UMidanRespawnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMidanRespawnComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!RaceRules.IsNull())
	{
		RulesLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			RaceRules.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &UMidanRespawnComponent::OnRulesLoaded));
	}
	else
	{
		UE_LOG(LogMidanRace, Warning, TEXT("UMidanRespawnComponent on '%s' has no RaceRules set — respawn will use no cooldown protection."), *GetNameSafe(GetOwner()));
	}

	if (!RespawnAction.IsNull())
	{
		RespawnActionLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			RespawnAction.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &UMidanRespawnComponent::OnRespawnActionLoaded));
	}
}

void UMidanRespawnComponent::OnRulesLoaded()
{
	LoadedRaceRules = RaceRules.Get();
}

void UMidanRespawnComponent::OnRespawnActionLoaded()
{
	TryBindRespawnInput();
}

void UMidanRespawnComponent::TryBindRespawnInput()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		// Correct on an AI pawn: opponents never respawn via input, only the
		// player does. An AI stuck against a wall is MidanAI's avoidance and
		// mistake-recovery problem (Phase 6), not this component's.
		return;
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(OwnerPawn->InputComponent);
	UInputAction* Action = RespawnAction.Get();
	if (!EnhancedInput || !Action)
	{
		return;
	}

	EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, &UMidanRespawnComponent::RequestRespawn);
}

bool UMidanRespawnComponent::IsOnCooldown() const
{
	return GetCooldownRemainingSeconds() > 0.f;
}

float UMidanRespawnComponent::GetCooldownRemainingSeconds() const
{
	if (CooldownEndsAtWorldTimeSeconds < 0.f || !GetWorld())
	{
		return 0.f;
	}
	return FMath::Max(0.f, CooldownEndsAtWorldTimeSeconds - GetWorld()->GetTimeSeconds());
}

void UMidanRespawnComponent::RequestRespawn()
{
	if (IsOnCooldown())
	{
		return;
	}

	AActor* Owner = GetOwner();
	IMidanTrackInterface* Track = nullptr;

	if (UMidanServiceLocatorSubsystem* Locator = GetWorld() ? GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>() : nullptr)
	{
		Track = Locator->GetTrack().GetInterface();
	}

	IMidanVehicleInterface* Vehicle = Owner ? Cast<IMidanVehicleInterface>(Owner) : nullptr;

	if (!Owner || !Track || !Vehicle)
	{
		UE_LOG(LogMidanRace, Warning, TEXT("UMidanRespawnComponent on '%s': cannot respawn — missing owner interface or no track registered."), *GetNameSafe(Owner));
		return;
	}

	PerformRespawn();

	const float CooldownSeconds = LoadedRaceRules ? LoadedRaceRules->RespawnCooldownSeconds : 5.f;
	CooldownEndsAtWorldTimeSeconds = GetWorld()->GetTimeSeconds() + CooldownSeconds;
}

void UMidanRespawnComponent::PerformRespawn()
{
	AActor* Owner = GetOwner();
	UMidanServiceLocatorSubsystem* Locator = GetWorld() ? GetWorld()->GetSubsystem<UMidanServiceLocatorSubsystem>() : nullptr;
	IMidanTrackInterface* Track = Locator ? Locator->GetTrack().GetInterface() : nullptr;
	IMidanVehicleInterface* Vehicle = Cast<IMidanVehicleInterface>(Owner);

	if (!Track || !Vehicle)
	{
		return;
	}

	const UMidanLapTimingSubsystem* LapTiming = GetWorld() ? GetWorld()->GetSubsystem<UMidanLapTimingSubsystem>() : nullptr;
	const float ArcLength = LapTiming ? LapTiming->GetRacerLastCheckpointArcLength(Owner) : 0.f;
	const FTransform SafeTransform = Track->GetTransformAtDistance(ArcLength);

	Vehicle->SetInputLocked(true);
	Owner->SetActorTransform(SafeTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		RootPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
		RootPrimitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	Vehicle->SetInputLocked(false);

	UE_LOG(LogMidanRace, Log, TEXT("'%s' respawned at arc length %.0fcm."), *Owner->GetName(), ArcLength);
}
