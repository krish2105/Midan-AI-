// The racer. Player-driven or AI-driven, identically.
//
// Responsibility: assemble the vehicle from its Data Asset and own the single
// input path into the solver.
// Single reason to change: the set of components a vehicle needs changes.
//
// Implements IMidanVehicleInterface, which is how MidanRace and MidanTelemetry
// observe a vehicle without depending on MidanVehicle, and
// IMidanTelemetrySource, which is how it gets captured at 60Hz.
//
// The same pawn class is used for the player and all seven opponents. The ONLY
// difference is who calls ApplyInput — UVehicleInputComponent for the player,
// AMidanOpponentController for the AI. There is no AI branch inside this class
// and there must never be one: master prompt §4.2 requires that the AI cannot
// cheat physics, and identical code paths is how that is guaranteed rather than
// merely intended.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "MidanVehicleInterface.h"
#include "MidanTelemetryInterfaces.h"
#include "MidanCoreTypes.h"
#include "MidanVehiclePawn.generated.h"

class UMidanInputConfigDataAsset;
class UMidanVehicleMovementComponent;
class USurfaceResponseDataAsset;
class UVehicleAeroComponent;
class UVehicleAssistComponent;
class UVehicleInputComponent;
class UVehicleSetupDataAsset;
class USpringArmComponent;
class UCameraComponent;

UCLASS(Abstract, Blueprintable)
class MIDANVEHICLE_API AMidanVehiclePawn
	: public AWheeledVehiclePawn
	, public IMidanVehicleInterface
	, public IMidanTelemetrySource
{
	GENERATED_BODY()

public:
	AMidanVehiclePawn();

	/**
	 * The vehicle's handling, in full. Soft, and async-loaded on BeginPlay.
	 *
	 * This asset is the source of truth: mass, powertrain, drivetrain,
	 * suspension, tyres, steering, aero and assists all come from here and
	 * nowhere else. The Chaos movement component is a downstream consumer.
	 *
	 * Set on the Blueprint subclass, which is the only thing a Blueprint in this
	 * project is for — carrying asset references.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|Setup")
	TSoftObjectPtr<UVehicleSetupDataAsset> VehicleSetup;

	/** Surface response table. Falls back to the project default in
	 *  UMidanDeveloperSettings when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|Setup")
	TSoftObjectPtr<USurfaceResponseDataAsset> SurfaceResponse;

	/** Player input actions. Unset on AI vehicles, which is correct — an
	 *  opponent has no input config because it is not driven by a device. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Midan|Setup")
	TSoftObjectPtr<UMidanInputConfigDataAsset> InputConfig;

	//~ IMidanVehicleInterface
	virtual void ApplyInput(const FMidanVehicleInputState& Input) override;
	virtual void GetVehicleFrameState(FMidanVehicleFrameState& OutState) const override;
	virtual FGameplayTag GetVehicleClassTag() const override;
	virtual float GetForwardSpeedKmh() const override;
	virtual const FMidanVehicleInputState& GetLastAppliedInput() const override { return LastAppliedInput; }
	virtual bool IsInputLocked() const override { return bInputLocked; }
	virtual void SetInputLocked(bool bLocked) override;

	//~ IMidanTelemetrySource
	virtual void CaptureTelemetryState(FMidanVehicleFrameState& OutState) const override;
	virtual void CaptureTelemetryInput(FMidanVehicleInputState& OutInput) const override;
	virtual FName GetTelemetrySourceId() const override { return TelemetrySourceId; }
	virtual bool IsTelemetryCaptureEnabled() const override { return bTelemetryCaptureEnabled; }

	/** Set by the race game mode when populating the grid, so the capture file
	 *  distinguishes the player from seven opponents. */
	void SetTelemetrySourceId(FName InId) { TelemetrySourceId = InId; }
	void SetTelemetryCaptureEnabled(bool bEnabled) { bTelemetryCaptureEnabled = bEnabled; }

	//~ Component accessors
	UFUNCTION(BlueprintPure, Category = "Midan|Vehicle")
	UMidanVehicleMovementComponent* GetMidanMovement() const { return MidanMovement; }

	UFUNCTION(BlueprintPure, Category = "Midan|Vehicle")
	UVehicleAssistComponent* GetAssists() const { return Assists; }

	UFUNCTION(BlueprintPure, Category = "Midan|Vehicle")
	UVehicleAeroComponent* GetAero() const { return Aero; }

	/** True once the setup asset has loaded and been applied. Until then the car
	 *  has engine defaults and must not be judged for feel. */
	UFUNCTION(BlueprintPure, Category = "Midan|Vehicle")
	bool IsSetupApplied() const { return bSetupApplied; }

	//~ AActor / APawn
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void UnPossessed() override;

protected:
	/**
	 * Async-load the setup asset, then apply it.
	 *
	 * Async because CLAUDE.md forbids synchronous loads and hard references. The
	 * consequence is real and must be designed around: for a frame or two after
	 * spawn the car has engine defaults. The race game mode holds vehicles in
	 * Race.State.Grid with input locked until IsSetupApplied is true, so nobody
	 * ever drives an unconfigured car.
	 */
	void RequestSetupLoad();
	void OnSetupLoaded();

	/** Apply the loaded asset to every component that needs it. */
	void ApplyLoadedSetup();

private:
	/** Chaos movement, replacing the base class's default component so the
	 *  assist hook exists. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Midan|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMidanVehicleMovementComponent> MidanMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Midan|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVehicleAeroComponent> Aero;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Midan|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UVehicleSurfaceSensorComponent> SurfaceSensor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Midan|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVehicleAssistComponent> Assists;

	/** Present on every vehicle but only bound for the player. An AI pawn keeps
	 *  an unbound one rather than a null, so there is no null check on a hot
	 *  path and no divergence between player and AI pawn composition. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Midan|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVehicleInputComponent> MidanInput;

	/** Resolved setup, valid once loaded. */
	UPROPERTY(Transient)
	TObjectPtr<const UVehicleSetupDataAsset> LoadedSetup;

	UPROPERTY(Transient)
	TObjectPtr<const USurfaceResponseDataAsset> LoadedSurfaceResponse;

	UPROPERTY(Transient)
	TObjectPtr<const UMidanInputConfigDataAsset> LoadedInputConfig;

	/** Handle for the async load, so a pawn destroyed mid-load cancels cleanly
	 *  rather than completing into a dangling this. */
	TSharedPtr<struct FStreamableHandle> SetupLoadHandle;

	FMidanVehicleInputState LastAppliedInput;

	FName TelemetrySourceId = NAME_None;

	bool bTelemetryCaptureEnabled = false;
	bool bSetupApplied = false;
	bool bInputLocked = true;
};
