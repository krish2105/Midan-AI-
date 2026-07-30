// The source of truth for how a car drives.
//
// Responsibility: hold every handling tuning value for one vehicle.
// Single reason to change: a new category of handling parameter is introduced.
//
// This asset is upstream; UChaosWheeledVehicleMovementComponent is a DOWNSTREAM
// CONSUMER. UVehicleSetupApplier writes these values into the Chaos component
// at BeginPlay and on editor hot-reload. Never hand-edit the Chaos component —
// the next apply overwrites it, and the change is invisible in version control.
//
// Why the indirection is worth it: vehicles become diffable in git, validatable
// in CI, tunable without a recompile, and — the reason the telemetry phase is
// possible at all — generatable and sweepable programmatically.

#pragma once

#include "CoreMinimal.h"
#include "MidanDataAsset.h"
#include "GameplayTagContainer.h"

#include "Config/VehicleMassConfig.h"
#include "Config/VehiclePowertrainConfig.h"
#include "Config/VehicleDrivetrainConfig.h"
#include "Config/VehicleSuspensionConfig.h"
#include "Config/VehicleTyreConfig.h"
#include "Config/VehicleSteeringConfig.h"
#include "Config/VehicleAeroConfig.h"
#include "Config/VehicleAssistConfig.h"

#include "VehicleSetupDataAsset.generated.h"

class UVehicleFeelDataAsset;
class USkeletalMesh;

UCLASS(BlueprintType)
class MIDANVEHICLE_API UVehicleSetupDataAsset : public UMidanDataAsset
{
	GENERATED_BODY()

public:
	// --- Identity ------------------------------------------------------------

	/**
	 * Player-facing vehicle name.
	 *
	 * FICTIONAL ONLY. No real manufacturer or model name, ever — this is a legal
	 * boundary, not a stylistic preference. See docs/ART_DIRECTION.md §0.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	/** Must be a descendant of Vehicle.Class. Validation enforces it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (Categories = "Vehicle.Class"))
	FGameplayTag VehicleClass;

	/** One-line handling personality, for the vehicle select screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = "true"))
	FText PersonalityDescription;

	// --- Handling ------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mass")
	FVehicleMassConfig Mass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Powertrain")
	FVehiclePowertrainConfig Powertrain;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drivetrain")
	FVehicleDrivetrainConfig Drivetrain;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suspension")
	FVehicleSuspensionConfig Suspension;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tyres")
	FVehicleTyreConfig Tyres;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering")
	FVehicleSteeringConfig Steering;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aero")
	FVehicleAeroConfig Aero;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Assists")
	FVehicleAssistConfig Assists;

	// --- References ----------------------------------------------------------

	/**
	 * Camera, audio, FX and haptics for this vehicle.
	 *
	 * Soft reference: the feel asset pulls in Niagara systems, sounds and
	 * force-feedback effects, and hard-referencing all of that from the setup
	 * asset would load every vehicle's FX when any vehicle loads.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feel")
	TSoftObjectPtr<UVehicleFeelDataAsset> Feel;

	/** Chassis skeletal mesh. Soft — vehicles are async-loaded on selection. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Art")
	TSoftObjectPtr<USkeletalMesh> ChassisMesh;

	// --- Derived accessors, for the UI and for tuning sweeps ------------------

	/** Power-to-weight in Nm per tonne at the torque peak. Comparative only. */
	UFUNCTION(BlueprintPure, Category = "Derived")
	float GetPeakTorquePerTonne() const;

	/** > 1.0 oversteer, < 1.0 understeer. The one number to sweep for balance. */
	UFUNCTION(BlueprintPure, Category = "Derived")
	float GetHandlingBalance() const { return Tyres.GetFrontRearFrictionRatio(); }

	/** Front share of total downforce. Below 0.5 is rear-biased and stable. */
	UFUNCTION(BlueprintPure, Category = "Derived")
	float GetAeroBalance() const { return Aero.GetFrontDownforceBalance(); }

	//~ Begin UMidanDataAsset interface
	virtual void ValidateMidanData(FMidanValidationResult& Result) const override;
	//~ End UMidanDataAsset interface

	//~ Begin UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UPrimaryDataAsset interface
};
