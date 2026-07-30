// Writes a vehicle setup Data Asset onto a Chaos movement component.
//
// Responsibility: data -> physics translation, and nothing else.
// Single reason to change: the Chaos parameter surface changes.
//
// This is the ONLY place in the project that writes to
// UChaosWheeledVehicleMovementComponent's tuning fields. The direction is
// strictly one-way: asset -> component. Nothing ever reads a tuned value back
// off the component and treats it as truth, because then the Data Asset would
// stop being the source of truth and the whole pipeline collapses.
//
// Do not hand-edit the Chaos component in a Blueprint. The next Apply
// overwrites it, and the edit is invisible in version control.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VehicleSetupApplier.generated.h"

class UVehicleSetupDataAsset;
class UChaosWheeledVehicleMovementComponent;
class UChaosVehicleWheel;

/**
 * Stateless applier. All members are static — there is no instance state to get
 * out of sync with the asset.
 */
UCLASS()
class MIDANVEHICLE_API UVehicleSetupApplier : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Write every value from Setup onto Movement.
	 *
	 * Called from AMidanVehiclePawn::BeginPlay at Phase 3, and from the editor
	 * hot-reload delegate below so a designer sees a change without a PIE
	 * restart. Iteration speed is the whole point of the data pipeline; a
	 * change that costs a recompile gets made an order of magnitude less often.
	 *
	 * Returns false and logs when either argument is null, or when Setup fails
	 * validation — applying a known-invalid setup produces NaN in the solver,
	 * which surfaces as the car vanishing rather than as an error.
	 */
	static bool Apply(const UVehicleSetupDataAsset* Setup, UChaosWheeledVehicleMovementComponent* Movement);

#if WITH_EDITOR
	/**
	 * Re-apply to every spawned vehicle using this asset.
	 *
	 * Registered against UObject::FPropertyChangedEvent in Phase 3. Editor-only:
	 * hot reload has no meaning in a shipped build, and the delegate machinery
	 * would be dead weight there.
	 */
	static void ReapplyToAllInstances(const UVehicleSetupDataAsset* ChangedSetup);
#endif

private:
	static void ApplyMass(const UVehicleSetupDataAsset* Setup, UChaosWheeledVehicleMovementComponent* Movement);
	static void ApplyPowertrain(const UVehicleSetupDataAsset* Setup, UChaosWheeledVehicleMovementComponent* Movement);
	static void ApplyDrivetrain(const UVehicleSetupDataAsset* Setup, UChaosWheeledVehicleMovementComponent* Movement);
	static void ApplyWheels(const UVehicleSetupDataAsset* Setup, UChaosWheeledVehicleMovementComponent* Movement);
	static void ApplySteering(const UVehicleSetupDataAsset* Setup, UChaosWheeledVehicleMovementComponent* Movement);
};
