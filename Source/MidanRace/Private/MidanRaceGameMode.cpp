#include "MidanRaceGameMode.h"

#include "AIController.h"
#include "EngineUtils.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerController.h"
#include "MidanCheckpoint.h"
#include "MidanGameplayTags.h"
#include "MidanGridSpline.h"
#include "MidanLapTimingSubsystem.h"
#include "MidanLogChannels.h"
#include "MidanRaceGameState.h"
#include "MidanRacePlayerState.h"
#include "MidanRaceRulesDataAsset.h"
#include "MidanVehicleInterface.h"
#include "TimerManager.h"

AMidanRaceGameMode::AMidanRaceGameMode()
{
	PrimaryActorTick.bCanEverTick = false;

	GameStateClass = AMidanRaceGameState::StaticClass();
	PlayerStateClass = AMidanRacePlayerState::StaticClass();
	OpponentControllerClass = AAIController::StaticClass();
}

void AMidanRaceGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (RaceRulesAsset.IsNull())
	{
		UE_LOG(LogMidanRace, Error, TEXT("AMidanRaceGameMode '%s' has no RaceRulesAsset set. The race cannot start."), *GetName());
		return;
	}

	RaceRulesLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		RaceRulesAsset.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &AMidanRaceGameMode::OnRaceRulesLoaded));
}

void AMidanRaceGameMode::OnRaceRulesLoaded()
{
	LoadedRaceRules = RaceRulesAsset.Get();
	if (!LoadedRaceRules)
	{
		UE_LOG(LogMidanRace, Error, TEXT("AMidanRaceGameMode '%s': RaceRulesAsset failed to resolve after async load."), *GetName());
		return;
	}

	GatherLevelActors();

	if (AMidanRaceGameState* GS = GetGameState<AMidanRaceGameState>())
	{
		GS->InitialiseRaceRules(LoadedRaceRules);
	}

	if (UMidanLapTimingSubsystem* LapTiming = GetWorld()->GetSubsystem<UMidanLapTimingSubsystem>())
	{
		TArray<AMidanCheckpoint*> RawCheckpoints;
		RawCheckpoints.Reserve(Checkpoints.Num());
		for (AMidanCheckpoint* Checkpoint : Checkpoints)
		{
			RawCheckpoints.Add(Checkpoint);
		}
		LapTiming->InitialiseForRace(LoadedRaceRules, RawCheckpoints);
		LapTiming->OnRacerFinished.AddUObject(this, &AMidanRaceGameMode::HandleRacerFinished);
	}

	RequestOpponentClassesLoad();
}

void AMidanRaceGameMode::GatherLevelActors()
{
	Checkpoints.Reset();
	for (TActorIterator<AMidanCheckpoint> It(GetWorld()); It; ++It)
	{
		Checkpoints.Add(*It);
	}

	GridSpline = nullptr;
	for (TActorIterator<AMidanGridSpline> It(GetWorld()); It; ++It)
	{
		GridSpline = *It;
		break;
	}

	if (!GridSpline)
	{
		UE_LOG(LogMidanRace, Error, TEXT("AMidanRaceGameMode '%s': no AMidanGridSpline found in the level. Racers cannot be positioned."), *GetName());
	}

	if (Checkpoints.Num() == 0)
	{
		UE_LOG(LogMidanRace, Error, TEXT("AMidanRaceGameMode '%s': no AMidanCheckpoint actors found. Laps cannot be timed."), *GetName());
	}
}

void AMidanRaceGameMode::RequestOpponentClassesLoad()
{
	if (OpponentVehicleClasses.Num() == 0)
	{
		UE_LOG(LogMidanRace, Warning, TEXT("AMidanRaceGameMode '%s': OpponentVehicleClasses is empty. Grid will have the player only."), *GetName());
		bRaceRulesReady = true;
		TryBeginCountdown();
		return;
	}

	TArray<FSoftObjectPath> Paths;
	Paths.Reserve(OpponentVehicleClasses.Num());
	for (const TSoftClassPtr<APawn>& SoftClass : OpponentVehicleClasses)
	{
		Paths.Add(SoftClass.ToSoftObjectPath());
	}

	OpponentClassesLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		Paths, FStreamableDelegate::CreateUObject(this, &AMidanRaceGameMode::OnOpponentClassesLoaded));
}

void AMidanRaceGameMode::OnOpponentClassesLoaded()
{
	SpawnOpponents();
	bRaceRulesReady = true;
	TryBeginCountdown();
}

void AMidanRaceGameMode::SpawnOpponents()
{
	if (!GridSpline || OpponentVehicleClasses.Num() == 0 || !GetWorld())
	{
		return;
	}

	for (int32 Slot = NextOpponentSlotIndex; Slot < GridSpline->SlotCount; ++Slot)
	{
		TSubclassOf<APawn> PawnClass = OpponentVehicleClasses[(Slot - 1) % OpponentVehicleClasses.Num()].Get();
		if (!PawnClass)
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		APawn* Opponent = GetWorld()->SpawnActor<APawn>(PawnClass, GridSpline->GetSlotTransform(Slot), SpawnParams);
		if (!Opponent)
		{
			UE_LOG(LogMidanRace, Error, TEXT("AMidanRaceGameMode '%s': failed to spawn opponent at slot %d."), *GetName(), Slot);
			continue;
		}

		// OpponentControllerClass defaults to plain AAIController and is
		// intended to be set to AMidanOpponentController (MidanAI) on the
		// Blueprint subclass — see the class comment. bWantsPlayerState gives
		// this controller an AMidanRacePlayerState via PlayerStateClass,
		// which is how it enters PlayerArray for position and results
		// tracking identically to the human player.
		const TSubclassOf<AAIController> ControllerClass = OpponentControllerClass ? OpponentControllerClass : AAIController::StaticClass();
		AAIController* AIController = GetWorld()->SpawnActor<AAIController>(ControllerClass, Opponent->GetActorTransform());
		if (AIController)
		{
			AIController->bWantsPlayerState = true;
			AIController->InitPlayerState();
			AIController->Possess(Opponent);
		}

		RegisterRacer(Opponent, Slot);
	}
}

void AMidanRaceGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	bPlayerReady = true;

	if (APawn* PlayerPawn = NewPlayer ? NewPlayer->GetPawn() : nullptr)
	{
		RegisterRacer(PlayerPawn, 0);
	}
	else
	{
		UE_LOG(LogMidanRace, Warning, TEXT("AMidanRaceGameMode '%s': player logged in with no pawn yet. DefaultPawnClass may be unset."), *GetName());
	}

	TryBeginCountdown();
}

void AMidanRaceGameMode::RegisterRacer(APawn* RacerPawn, int32 GridSlotIndex)
{
	if (!RacerPawn)
	{
		return;
	}

	IMidanVehicleInterface* Vehicle = Cast<IMidanVehicleInterface>(RacerPawn);
	if (!Vehicle)
	{
		UE_LOG(LogMidanRace, Error, TEXT("AMidanRaceGameMode '%s': pawn '%s' does not implement IMidanVehicleInterface. Not registered as a racer."), *GetName(), *RacerPawn->GetName());
		return;
	}

	// Held locked from spawn through Race.State.Grid and Countdown —
	// OnCountdownFinished is the only place input unlocks. A car left to roll
	// on a sloped grid before the lights is a bug players notice immediately.
	Vehicle->SetInputLocked(true);

	if (GridSpline && GridSlotIndex >= 0)
	{
		RacerPawn->SetActorTransform(GridSpline->GetSlotTransform(GridSlotIndex), false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (UMidanLapTimingSubsystem* LapTiming = GetWorld()->GetSubsystem<UMidanLapTimingSubsystem>())
	{
		LapTiming->RegisterRacer(RacerPawn);
	}

	if (AController* RacerController = RacerPawn->GetController())
	{
		if (AMidanRacePlayerState* RacePS = RacerController->GetPlayerState<AMidanRacePlayerState>())
		{
			RacePS->BindToLapTiming(RacerPawn);
		}
	}

	RegisteredRacers.Add(RacerPawn);

	if (GridSlotIndex >= NextOpponentSlotIndex)
	{
		NextOpponentSlotIndex = GridSlotIndex + 1;
	}
}

void AMidanRaceGameMode::TryBeginCountdown()
{
	if (bCountdownStarted || !bRaceRulesReady || !bPlayerReady)
	{
		return;
	}

	bCountdownStarted = true;
	BeginCountdownSequence();
}

void AMidanRaceGameMode::BeginCountdownSequence()
{
	AMidanRaceGameState* GS = GetGameState<AMidanRaceGameState>();
	if (!GS || !LoadedRaceRules)
	{
		return;
	}

	GS->SetRaceStateTag(MidanTags::Race_State_Countdown);
	GS->BeginCountdown(LoadedRaceRules->CountdownDurationSeconds);

	GetWorldTimerManager().SetTimer(CountdownTimerHandle, this, &AMidanRaceGameMode::OnCountdownFinished, LoadedRaceRules->CountdownDurationSeconds, false);
}

void AMidanRaceGameMode::OnCountdownFinished()
{
	AMidanRaceGameState* GS = GetGameState<AMidanRaceGameState>();
	if (!GS)
	{
		return;
	}

	GS->SetRaceStateTag(MidanTags::Race_State_Racing);
	GS->MarkRaceStarted();

	if (UMidanLapTimingSubsystem* LapTiming = GetWorld()->GetSubsystem<UMidanLapTimingSubsystem>())
	{
		LapTiming->StartRace();
	}

	for (AActor* Racer : RegisteredRacers)
	{
		if (IMidanVehicleInterface* Vehicle = Racer ? Cast<IMidanVehicleInterface>(Racer) : nullptr)
		{
			Vehicle->SetInputLocked(false);
		}
	}
}

void AMidanRaceGameMode::HandleRacerFinished(AActor* Racer, int32 TotalLaps)
{
	if (bRaceFinishedTriggered)
	{
		return;
	}

	// The player finishing ends the race for the vertical slice's purposes.
	// Waiting for all eight would deadlock in this phase: opponents are
	// stationary placeholders (AMidanOpponentController arrives Phase 6) and
	// will never cross a checkpoint. See the class comment.
	APlayerController* LocalPlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!LocalPlayerController || LocalPlayerController->GetPawn() != Racer)
	{
		return;
	}

	bRaceFinishedTriggered = true;

	AMidanRaceGameState* GS = GetGameState<AMidanRaceGameState>();
	if (!GS || !LoadedRaceRules)
	{
		return;
	}

	GS->SetRaceStateTag(MidanTags::Race_State_Finished);
	GS->MarkRaceFinished();

	for (AActor* RegisteredRacer : RegisteredRacers)
	{
		if (IMidanVehicleInterface* Vehicle = RegisteredRacer ? Cast<IMidanVehicleInterface>(RegisteredRacer) : nullptr)
		{
			Vehicle->SetInputLocked(true);
		}
	}

	GetWorldTimerManager().SetTimer(ResultsDelayTimerHandle, this, &AMidanRaceGameMode::OnResultsDelayElapsed, LoadedRaceRules->ResultsDelaySeconds, false);
}

void AMidanRaceGameMode::OnResultsDelayElapsed()
{
	if (AMidanRaceGameState* GS = GetGameState<AMidanRaceGameState>())
	{
		GS->SetRaceStateTag(MidanTags::Race_State_Results);
	}
}

void AMidanRaceGameMode::Logout(AController* Exiting)
{
	if (APawn* ExitingPawn = Exiting ? Exiting->GetPawn() : nullptr)
	{
		RegisteredRacers.RemoveSingleSwap(ExitingPawn);
		if (UMidanLapTimingSubsystem* LapTiming = GetWorld()->GetSubsystem<UMidanLapTimingSubsystem>())
		{
			LapTiming->UnregisterRacer(ExitingPawn);
		}
	}

	Super::Logout(Exiting);
}
