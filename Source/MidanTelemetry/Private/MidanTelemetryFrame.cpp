#include "MidanTelemetryFrame.h"

void FMidanTelemetryFrame::BuildFromSource(
	const FMidanVehicleFrameState& VehicleState,
	const FMidanVehicleInputState& Input,
	double InTimestampSeconds,
	float InTrackArcLengthCm,
	float InTrackLateralOffsetCm,
	int32 InLapIndex,
	int32 InSectorIndex,
	FName InSourceId)
{
	TimestampSeconds = InTimestampSeconds;
	Location = VehicleState.Transform.GetLocation();
	Rotation = VehicleState.Transform.GetRotation();
	LinearVelocity = VehicleState.LinearVelocity;

	ForwardSpeedKmh = VehicleState.ForwardSpeedKmh;
	EngineRPM = VehicleState.EngineRPM;
	Gear = VehicleState.Gear;
	LateralG = VehicleState.LateralG;
	LongitudinalG = VehicleState.LongitudinalG;
	ChassisSlipAngleDegrees = VehicleState.ChassisSlipAngleDegrees;

	for (int32 i = 0; i < MidanTelemetryConstants::NumWheels; ++i)
	{
		const FMidanWheelState& Source = VehicleState.Wheels[i];
		FMidanTelemetryWheelSample& Dest = Wheels[i];

		Dest.SlipRatio = Source.SlipRatio;
		Dest.SlipAngleDegrees = Source.SlipAngleDegrees;
		Dest.NormalLoadN = Source.NormalLoadN;
		Dest.SurfaceTagName = Source.SurfaceTag.GetTagName();
		Dest.bInContact = Source.bInContact;
		Dest.bOffTrack = Source.bOffTrack;
	}

	Throttle = Input.Throttle;
	Brake = Input.Brake;
	Steer = Input.Steer;
	Handbrake = Input.Handbrake;

	TrackArcLengthCm = InTrackArcLengthCm;
	TrackLateralOffsetCm = InTrackLateralOffsetCm;
	LapIndex = InLapIndex;
	SectorIndex = InSectorIndex;
	SourceId = InSourceId;
}

FArchive& operator<<(FArchive& Ar, FMidanTelemetryFrame& Frame)
{
	Ar << Frame.TimestampSeconds;
	Ar << Frame.Location;
	Ar << Frame.Rotation;
	Ar << Frame.LinearVelocity;
	Ar << Frame.ForwardSpeedKmh;
	Ar << Frame.EngineRPM;
	Ar << Frame.Gear;
	Ar << Frame.LateralG;
	Ar << Frame.LongitudinalG;
	Ar << Frame.ChassisSlipAngleDegrees;

	for (FMidanTelemetryWheelSample& Wheel : Frame.Wheels)
	{
		Ar << Wheel;
	}

	Ar << Frame.Throttle;
	Ar << Frame.Brake;
	Ar << Frame.Steer;
	Ar << Frame.Handbrake;
	Ar << Frame.TrackArcLengthCm;
	Ar << Frame.TrackLateralOffsetCm;
	Ar << Frame.LapIndex;
	Ar << Frame.SectorIndex;
	Ar << Frame.SourceId;

	return Ar;
}
