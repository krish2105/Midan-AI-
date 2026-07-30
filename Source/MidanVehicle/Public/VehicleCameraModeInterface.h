// Camera mode contract — the four modes behind one interface.
//
// Responsibility: turn vehicle state plus a mode config into a camera view.
// Single reason to change: a camera behaviour is added or removed.
//
// DELIBERATE DEVIATION from docs/PHASE_PLAN.md, which listed four mode files —
// ChaseFar, ChaseNear, Bonnet, Cockpit. There are only TWO behaviours here:
//
//   FMidanBoomCameraMode   — a lagging spring arm behind the car.
//                            ChaseFar and ChaseNear differ ONLY in
//                            FMidanCameraModeConfig values (arm length, FOV,
//                            lag curves). Two classes would be identical code
//                            with different data, which is duplication, not
//                            configurability.
//
//   FMidanRigidCameraMode  — attached to the chassis, no arm, no positional lag.
//                            Bonnet and Cockpit likewise differ only in data.
//
// The mode COUNT is still four, and the player still cycles four. What changed
// is that the variation lives entirely in the Data Asset — which is the actual
// rule in CLAUDE.md. Recorded as docs/ASSUMPTIONS.md A29.
//
// These are PLAIN C++ TYPES, not UObjects. They take vehicle state in and give
// a transform out, with no UWorld dependency, so an Automation Spec can assert
// that a 30 degree slip angle yields the authored slip yaw without spawning a
// car. Pure math belongs in world-free types (CLAUDE.md).

#pragma once

#include "CoreMinimal.h"
#include "VehicleFeelDataAsset.h"

/**
 * Everything a camera mode is allowed to know about the vehicle this frame.
 *
 * Deliberately a flat snapshot rather than a pawn pointer. A mode that could
 * reach the pawn could also change it, and a camera must never perturb the
 * simulation it is observing.
 */
struct MIDANVEHICLE_API FMidanCameraViewInput
{
	/** Chassis transform this frame. */
	FTransform VehicleTransform = FTransform::Identity;

	/** Signed forward speed, km/h. Negative in reverse. */
	float ForwardSpeedKmh = 0.f;

	/** Speed normalised to [0,1] against FeelAsset SpeedNormalisationKmh. Every
	 *  speed-driven curve in the feel asset shares this normalisation, so they
	 *  can be authored against a common X axis. */
	float NormalisedSpeed = 0.f;

	/** Shaped steering, -1..1. Drives look-ahead yaw. */
	float SteerInput = 0.f;

	/** Chassis slip angle in degrees, signed. Drives slip yaw — this is what
	 *  makes a drift legible instead of looking like a bug. */
	float ChassisSlipAngleDegrees = 0.f;

	/** Mean surface roughness under grounded wheels, 0..1. Drives rumble. */
	float SurfaceRoughness = 0.f;

	/** Impulse magnitude from a collision this frame, 0 when none. Consumed
	 *  once — the component clears it after passing it in. */
	float ImpactImpulse = 0.f;

	/** True when the vehicle is fully airborne. Suppresses surface rumble;
	 *  there is no surface to be rough. */
	bool bAirborne = false;
};

/**
 * Camera state carried between frames.
 *
 * Persisting this is what makes lag, damping and shake decay possible at all —
 * every one of those behaviours is defined by the difference between where the
 * camera is and where it wants to be. A stateless camera cannot lag.
 */
struct MIDANVEHICLE_API FMidanCameraViewState
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float FOVDegrees = 72.f;

	/** Damped vertical position, used to filter suspension chatter without
	 *  losing real impacts. */
	float SmoothedHeightCm = 0.f;

	/** Current look-ahead and slip yaw, damped toward their targets rather than
	 *  applied instantly — an instant yaw snap on a steering input reads as the
	 *  camera twitching. */
	float LookAheadYawDegrees = 0.f;
	float SlipYawDegrees = 0.f;

	/** Decaying shake amplitude in degrees, fed by impacts. */
	float ShakeAmplitudeDegrees = 0.f;

	/** Continuous rumble amplitude in degrees, from surface roughness. */
	float RumbleAmplitudeDegrees = 0.f;

	/** Phase accumulator for the rumble oscillator. Kept in state so rumble is
	 *  continuous across frames instead of restarting each tick. */
	float RumblePhase = 0.f;

	/** False until the first update, so the camera snaps into place on the first
	 *  frame instead of lagging in from the world origin. */
	bool bInitialised = false;
};

/**
 * A camera mode. Pure function of (input, config, previous state) -> new state.
 */
class MIDANVEHICLE_API IMidanVehicleCameraMode
{
public:
	virtual ~IMidanVehicleCameraMode() = default;

	/** Human-readable, for logging and the Phase 4 gate report. */
	virtual const TCHAR* GetModeName() const = 0;

	/**
	 * Advance the camera one frame.
	 *
	 * DeltaSeconds must be the real frame delta. Every damping term inside uses
	 * MidanMath::ExpDamp rather than Lerp(a, b, rate * dt), because the naive
	 * form changes its effective smoothing with frame time — a camera tuned at
	 * 60fps then behaves differently at 144fps, which is a feel bug that hides
	 * behind frame-rate dependence and is miserable to find.
	 */
	virtual void UpdateView(
		const FMidanCameraViewInput& Input,
		const FMidanCameraModeConfig& Config,
		const UVehicleFeelDataAsset& Feel,
		float DeltaSeconds,
		FMidanCameraViewState& InOutState) const = 0;
};

/**
 * Chase camera on a lagging boom. Used by ChaseFar and ChaseNear.
 *
 * Implements all seven behaviours: speed FOV, positional lag, rotational lag,
 * look-ahead yaw, slip yaw, vertical damping, and impact shake plus surface
 * rumble.
 */
class MIDANVEHICLE_API FMidanBoomCameraMode final : public IMidanVehicleCameraMode
{
public:
	virtual const TCHAR* GetModeName() const override { return TEXT("Boom"); }

	virtual void UpdateView(
		const FMidanCameraViewInput& Input,
		const FMidanCameraModeConfig& Config,
		const UVehicleFeelDataAsset& Feel,
		float DeltaSeconds,
		FMidanCameraViewState& InOutState) const override;
};

/**
 * Camera rigidly attached to the chassis. Used by Bonnet and Cockpit.
 *
 * No positional lag and no boom — the camera IS the car, so lag would read as
 * the bonnet sliding around. It still gets speed FOV, slip yaw, vertical
 * damping and shake, because those are what make a hood camera feel fast rather
 * than merely close.
 *
 * Vertical damping matters MORE here, not less: a rigid camera receives the full
 * suspension chatter directly, and unfiltered chatter at head height is
 * genuinely nauseating.
 */
class MIDANVEHICLE_API FMidanRigidCameraMode final : public IMidanVehicleCameraMode
{
public:
	virtual const TCHAR* GetModeName() const override { return TEXT("Rigid"); }

	virtual void UpdateView(
		const FMidanCameraViewInput& Input,
		const FMidanCameraModeConfig& Config,
		const UVehicleFeelDataAsset& Feel,
		float DeltaSeconds,
		FMidanCameraViewState& InOutState) const override;
};

namespace MidanCameraMath
{
	/**
	 * Shared behaviours, factored out so the two modes cannot drift apart.
	 *
	 * These are the parts where "the bonnet camera feels different from the
	 * chase camera in a way nobody intended" would otherwise come from.
	 */

	/** FOV from base plus the speed curve, clamped to a sane lens range. */
	MIDANVEHICLE_API float ComputeFOV(const FMidanCameraModeConfig& Config, float NormalisedSpeed);

	/** Look-ahead yaw target from steering, in degrees. */
	MIDANVEHICLE_API float ComputeLookAheadTarget(const FMidanCameraModeConfig& Config, float SteerInput);

	/** Slip yaw target from chassis slip angle, in degrees, clamped to the
	 *  authored maximum. */
	MIDANVEHICLE_API float ComputeSlipYawTarget(const FMidanCameraModeConfig& Config, float ChassisSlipAngleDegrees);

	/** Advance shake and rumble amplitudes, and return the combined angular
	 *  offset to apply this frame. */
	MIDANVEHICLE_API FRotator UpdateShakeAndRumble(
		const FMidanCameraModeConfig& Config,
		const UVehicleFeelDataAsset& Feel,
		const FMidanCameraViewInput& Input,
		float DeltaSeconds,
		FMidanCameraViewState& InOutState);

	/**
	 * Slip angle treated as "fully sideways" for camera purposes, degrees.
	 *
	 * A definition rather than a tuning value: it is the denominator that makes
	 * FMidanCameraModeConfig::SlipYawDegrees mean "yaw at full slide". Changing
	 * it would silently rescale every authored SlipYawDegrees. Matches the
	 * constant used by UVehicleAssistComponent for the same reason.
	 */
	static constexpr float FullySidewaysAngleDegrees = 45.f;

	/** Rate at which shake amplitude decays, 1/seconds. Not a feel dial: it is
	 *  fast enough that a shake reads as an impact rather than a wobble, and the
	 *  authored magnitude comes from ImpactShakeByImpulse. */
	static constexpr float ShakeDecayRate = 6.f;

	/** Oscillations per second for the rumble noise. A definition of "rumble"
	 *  rather than a tuning value — the tunable part is the amplitude curve. */
	static constexpr float RumbleFrequencyHz = 28.f;
}
