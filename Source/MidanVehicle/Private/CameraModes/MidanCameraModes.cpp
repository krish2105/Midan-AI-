// The two camera behaviours. See VehicleCameraModeInterface.h for why there are
// two rather than the four listed in docs/PHASE_PLAN.md.

#include "VehicleCameraModeInterface.h"

#include "MidanCurveUtils.h"
#include "MidanMathUtils.h"

namespace MidanCameraMath
{
	float ComputeFOV(const FMidanCameraModeConfig& Config, const float NormalisedSpeed)
	{
		// THE SINGLE BIGGEST SPEED-SENSATION LEVER. A tight base widening under
		// speed does more for the feeling of velocity than any particle effect.
		// Fallback of 0 means an unauthored curve gives a fixed FOV rather than
		// something invented — an unauthored curve is already a validation
		// warning, and inventing a widening here would hide it.
		const float Offset = MidanCurve::EvalSafe(Config.FOVOffsetBySpeed, NormalisedSpeed, 0.f);

		// Clamped unconditionally. An unclamped FOV produces a spectacular
		// failure exactly once, in front of someone important.
		return FMath::Clamp(Config.BaseFOVDegrees + Offset, 40.f, 140.f);
	}

	float ComputeLookAheadTarget(const FMidanCameraModeConfig& Config, const float SteerInput)
	{
		// Linear in steering: the camera leads the corner in proportion to how
		// hard the driver is asking for it. Reads as the camera anticipating
		// rather than following.
		return FMath::Clamp(SteerInput, -1.f, 1.f) * Config.LookAheadYawDegrees;
	}

	float ComputeSlipYawTarget(const FMidanCameraModeConfig& Config, const float ChassisSlipAngleDegrees)
	{
		// Normalised against "fully sideways" so SlipYawDegrees reads as the yaw
		// at a full slide. Without this the camera stays square to the chassis
		// during a drift and the slide is invisible to the player — the car just
		// appears to travel diagonally for no reason.
		const float Normalised = FMath::Clamp(
			ChassisSlipAngleDegrees / FullySidewaysAngleDegrees, -1.f, 1.f);

		return Normalised * Config.SlipYawDegrees;
	}

	FRotator UpdateShakeAndRumble(
		const FMidanCameraModeConfig& Config,
		const UVehicleFeelDataAsset& Feel,
		const FMidanCameraViewInput& Input,
		const float DeltaSeconds,
		FMidanCameraViewState& InOutState)
	{
		// --- Impact shake. Amplitude comes from the impulse curve, so a light
		// kerb strike and a wall hit do not feel identical. Unscaled shake makes
		// every impact feel the same, which means none of them feel big.
		if (Input.ImpactImpulse > KINDA_SMALL_NUMBER)
		{
			const float NewShake = MidanCurve::EvalSafe(Feel.ImpactShakeByImpulse, Input.ImpactImpulse, 0.f);

			// Take the max rather than accumulating: two impacts in quick
			// succession should not sum into a camera that leaves the screen.
			InOutState.ShakeAmplitudeDegrees = FMath::Max(InOutState.ShakeAmplitudeDegrees, NewShake);
		}

		// Exponential decay, frame-rate independent.
		InOutState.ShakeAmplitudeDegrees = MidanMath::ExpDamp(
			InOutState.ShakeAmplitudeDegrees, 0.f, Feel.ShakeDecayRate, DeltaSeconds);

		// --- Surface rumble. Continuous low-amplitude texture the player reads
		// without noticing it. Suppressed in the air, because there is no
		// surface to be rough.
		const float TargetRumble = Input.bAirborne
			? 0.f
			: MidanCurve::EvalSafe(Feel.SurfaceRumbleByRoughness, Input.SurfaceRoughness, 0.f);

		// Damped rather than snapped, so crossing onto gravel fades in over a
		// few frames instead of the camera jolting at the boundary.
		InOutState.RumbleAmplitudeDegrees = MidanMath::ExpDamp(
			InOutState.RumbleAmplitudeDegrees, TargetRumble, Feel.RumbleFadeRate, DeltaSeconds);

		InOutState.RumblePhase += DeltaSeconds * RumbleFrequencyHz * 2.f * PI;
		if (InOutState.RumblePhase > 2.f * PI * 1024.f)
		{
			// Wrap to keep float precision usable over a long session. Without
			// this the phase eventually grows large enough that the oscillator
			// quantises and the rumble becomes a buzz.
			InOutState.RumblePhase = FMath::Fmod(InOutState.RumblePhase, 2.f * PI);
		}

		// Two incommensurable frequencies per axis so the motion does not read as
		// a clean sine wave, which the eye picks up as mechanical.
		const float P = InOutState.RumblePhase;
		const float Rumble = InOutState.RumbleAmplitudeDegrees;
		const float Shake = InOutState.ShakeAmplitudeDegrees;

		FRotator Offset = FRotator::ZeroRotator;
		Offset.Pitch = Rumble * FMath::Sin(P * 1.00f) + Shake * FMath::Sin(P * 0.61f);
		Offset.Yaw   = Rumble * FMath::Sin(P * 0.73f) + Shake * FMath::Sin(P * 0.47f);
		Offset.Roll  = Rumble * FMath::Sin(P * 1.31f) * 0.5f + Shake * FMath::Sin(P * 0.89f) * 0.5f;

		return Offset;
	}

	/** Vertical damping shared by both modes. Filters high-frequency suspension
	 *  chatter while letting large motions through. */
	static float ApplyVerticalDamping(
		const float TargetHeightCm,
		const FMidanCameraModeConfig& Config,
		const float DeltaSeconds,
		float& InOutSmoothedHeightCm)
	{
		// VerticalDamping 0 passes everything, 1 damps hard. Mapped onto a rate
		// so the authored value stays a 0..1 dial rather than a raw frequency.
		//
		// The point is selective: suspension chatter reaching the camera is
		// nauseating, but a real impact must still land. A low rate filters the
		// fast component and leaves the slow one, which is exactly that split.
		const float Rate = FMath::Lerp(
			Config.VerticalDampingRateMax,
			Config.VerticalDampingRateMin,
			FMath::Clamp(Config.VerticalDamping, 0.f, 1.f));

		InOutSmoothedHeightCm = MidanMath::ExpDamp(
			InOutSmoothedHeightCm, TargetHeightCm, Rate, DeltaSeconds);

		return InOutSmoothedHeightCm;
	}
}

void FMidanBoomCameraMode::UpdateView(
	const FMidanCameraViewInput& Input,
	const FMidanCameraModeConfig& Config,
	const UVehicleFeelDataAsset& Feel,
	const float DeltaSeconds,
	FMidanCameraViewState& InOutState) const
{
	using namespace MidanCameraMath;

	const FVector VehicleLoc = Input.VehicleTransform.GetLocation();
	const FRotator VehicleRot = Input.VehicleTransform.Rotator();

	// --- Yaw behaviours, damped toward their targets.
	//
	// Damped rather than applied instantly because an instant yaw on a steering
	// input reads as the camera twitching, and the whole point of look-ahead is
	// that it feels like anticipation rather than reaction.
	const float LookAheadTarget = ComputeLookAheadTarget(Config, Input.SteerInput);
	const float SlipYawTarget = ComputeSlipYawTarget(Config, Input.ChassisSlipAngleDegrees);

	InOutState.LookAheadYawDegrees = MidanMath::ExpDamp(
		InOutState.LookAheadYawDegrees, LookAheadTarget, Config.LookAheadDampingRate, DeltaSeconds);
	InOutState.SlipYawDegrees = MidanMath::ExpDamp(
		InOutState.SlipYawDegrees, SlipYawTarget, Config.SlipYawDampingRate, DeltaSeconds);

	// Desired boom direction: behind the car, offset by look-ahead and slip.
	FRotator DesiredRot = VehicleRot;
	DesiredRot.Yaw += InOutState.LookAheadYawDegrees + InOutState.SlipYawDegrees;
	DesiredRot.Pitch = Config.PitchDegrees;
	DesiredRot.Roll = 0.f; // Never inherit chassis roll on a chase camera — it is
	                       // disorienting and hides the horizon reference.

	// --- Positional target: behind and above, along the DESIRED yaw rather than
	// the chassis yaw, so the boom swings out during a slide instead of staying
	// glued behind the tail.
	const FVector BoomDir = FRotator(0.f, DesiredRot.Yaw, 0.f).Vector();
	FVector DesiredLoc = VehicleLoc - BoomDir * Config.ArmLengthCm;

	// Vertical handled separately so it can be damped independently of the
	// horizontal chase.
	const float TargetHeight = VehicleLoc.Z + Config.HeightCm;
	if (!InOutState.bInitialised)
	{
		InOutState.SmoothedHeightCm = TargetHeight;
	}
	DesiredLoc.Z = ApplyVerticalDamping(
		TargetHeight, Config, DeltaSeconds, InOutState.SmoothedHeightCm);

	// --- Lag. This is what gives the car apparent mass: the camera is chasing
	// something heavy rather than rigidly following it.
	//
	// Fallbacks match ART_DIRECTION §5's mid-range values so an unauthored curve
	// still produces a usable camera rather than an instant-snap one.
	const float LocationLagRate = MidanCurve::EvalSafe(Config.LocationLagBySpeed, Input.NormalisedSpeed, 10.f);
	const float RotationLagRate = MidanCurve::EvalSafe(Config.RotationLagBySpeed, Input.NormalisedSpeed, 7.5f);

	if (!InOutState.bInitialised)
	{
		// Snap on the first frame. Without this the camera lags in from the
		// world origin, which is a visible glitch at every race start.
		InOutState.Location = DesiredLoc;
		InOutState.Rotation = DesiredRot;
		InOutState.bInitialised = true;
	}
	else
	{
		InOutState.Location = MidanMath::ExpDamp(InOutState.Location, DesiredLoc, LocationLagRate, DeltaSeconds);

		// Rotation damped as a quaternion slerp rather than per-Euler-component:
		// damping Euler angles independently gimbals near the poles and wobbles
		// through a 180 degree yaw wrap.
		const FQuat Current(InOutState.Rotation);
		const FQuat Desired(DesiredRot);
		const float Alpha = 1.f - FMath::Exp(-RotationLagRate * DeltaSeconds);
		InOutState.Rotation = FQuat::Slerp(Current, Desired, FMath::Clamp(Alpha, 0.f, 1.f)).Rotator();
	}

	InOutState.FOVDegrees = ComputeFOV(Config, Input.NormalisedSpeed);

	// Shake and rumble applied on top of the damped rotation, never fed back into
	// it — otherwise the damping would smooth the shake away and it would do
	// nothing.
	InOutState.Rotation += UpdateShakeAndRumble(Config, Feel, Input, DeltaSeconds, InOutState);
}

void FMidanRigidCameraMode::UpdateView(
	const FMidanCameraViewInput& Input,
	const FMidanCameraModeConfig& Config,
	const UVehicleFeelDataAsset& Feel,
	const float DeltaSeconds,
	FMidanCameraViewState& InOutState) const
{
	using namespace MidanCameraMath;

	// Attached to the chassis. ArmLengthCm is read as a FORWARD offset here
	// rather than a boom length — positive puts the camera ahead of the origin
	// for a bonnet view, near zero or negative for a cockpit view.
	const FTransform& VT = Input.VehicleTransform;
	const FVector Forward = VT.GetUnitAxis(EAxis::X);

	FVector DesiredLoc = VT.GetLocation() + Forward * Config.ArmLengthCm;

	const float TargetHeight = VT.GetLocation().Z + Config.HeightCm;
	if (!InOutState.bInitialised)
	{
		InOutState.SmoothedHeightCm = TargetHeight;
	}

	// Vertical damping matters MORE here than on the boom, not less: a rigid
	// camera receives the full suspension chatter directly, and unfiltered
	// chatter at head height is genuinely nauseating.
	DesiredLoc.Z = ApplyVerticalDamping(
		TargetHeight, Config, DeltaSeconds, InOutState.SmoothedHeightCm);

	// No positional lag: the camera IS the car. Lag would read as the bonnet
	// sliding around independently of the bodywork.
	InOutState.Location = DesiredLoc;

	// Slip yaw still applies, and is arguably more valuable here — from inside
	// the car a slide is otherwise almost impossible to read.
	const float SlipYawTarget = ComputeSlipYawTarget(Config, Input.ChassisSlipAngleDegrees);
	InOutState.SlipYawDegrees = MidanMath::ExpDamp(
		InOutState.SlipYawDegrees, SlipYawTarget, Config.SlipYawDampingRate, DeltaSeconds);

	// Look-ahead is deliberately NOT applied. From a fixed head position, a yaw
	// that leads the steering reads as the driver's head turning on its own.
	InOutState.LookAheadYawDegrees = 0.f;

	FRotator DesiredRot = VT.Rotator();
	DesiredRot.Yaw += InOutState.SlipYawDegrees;
	DesiredRot.Pitch += Config.PitchDegrees;
	// Chassis roll IS inherited here, unlike the boom: from inside the car, body
	// roll through a corner is a large part of what communicates load.

	InOutState.Rotation = DesiredRot;
	InOutState.bInitialised = true;

	InOutState.FOVDegrees = ComputeFOV(Config, Input.NormalisedSpeed);
	InOutState.Rotation += UpdateShakeAndRumble(Config, Feel, Input, DeltaSeconds, InOutState);
}
