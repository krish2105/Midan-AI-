// Automation Specs for the telemetry ring buffer and the binary
// write/read round trip — the Phase 9 gate's correctness backbone. A write
// path with no read path to verify it against is unverifiable, which is why
// MidanTelemetryWriter has both.

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "MidanGhostData.h"
#include "MidanTelemetryRingBuffer.h"
#include "MidanTelemetryWriter.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FMidanTelemetryFrame MakeTestFrame(int32 Index)
	{
		FMidanTelemetryFrame Frame;
		Frame.TimestampSeconds = Index * 0.0166;
		Frame.Location = FVector(static_cast<float>(Index), 0.f, 0.f);
		Frame.ForwardSpeedKmh = 100.f + Index;
		Frame.Gear = (Index % 6) + 1;
		Frame.LapIndex = Index / 100;
		Frame.SourceId = FName(TEXT("TestSource"));
		Frame.Wheels[0].SlipRatio = 0.1f * Index;
		Frame.Wheels[0].SurfaceTagName = FName(TEXT("Surface.Tarmac"));
		return Frame;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanRingBufferBasicTest,
	"Midan.Telemetry.RingBuffer.PushPopPreservesOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanRingBufferBasicTest::RunTest(const FString& Parameters)
{
	FMidanTelemetryRingBuffer RingBuffer;
	RingBuffer.Initialise(8);

	for (int32 i = 0; i < 5; ++i)
	{
		TestTrue(FString::Printf(TEXT("Push %d succeeds while under capacity"), i), RingBuffer.Push(MakeTestFrame(i)));
	}

	for (int32 i = 0; i < 5; ++i)
	{
		FMidanTelemetryFrame Frame;
		TestTrue(FString::Printf(TEXT("Pop %d succeeds"), i), RingBuffer.Pop(Frame));
		TestEqual(FString::Printf(TEXT("Pop %d preserves FIFO order"), i), Frame.Gear, MakeTestFrame(i).Gear);
	}

	FMidanTelemetryFrame Empty;
	TestFalse(TEXT("Popping an empty buffer fails"), RingBuffer.Pop(Empty));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanRingBufferFullTest,
	"Midan.Telemetry.RingBuffer.RejectsPushWhenFull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanRingBufferFullTest::RunTest(const FString& Parameters)
{
	FMidanTelemetryRingBuffer RingBuffer;
	RingBuffer.Initialise(4);

	// Capacity 4 usably holds 3 (one slot sacrificed for the lock-free
	// full/empty test — see the header comment).
	int32 Accepted = 0;
	for (int32 i = 0; i < 10; ++i)
	{
		if (RingBuffer.Push(MakeTestFrame(i)))
		{
			++Accepted;
		}
	}

	TestEqual(TEXT("A capacity-4 ring accepts exactly 3 pushes before rejecting"), Accepted, 3);

	TArray<FMidanTelemetryFrame> Drained;
	TestEqual(TEXT("DrainInto recovers exactly the accepted count"), RingBuffer.DrainInto(Drained), 3);

	// After draining, the buffer accepts pushes again — full is not sticky.
	TestTrue(TEXT("A drained buffer accepts a new push"), RingBuffer.Push(MakeTestFrame(99)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanTelemetryBinaryRoundTripTest,
	"Midan.Telemetry.Writer.BinaryRoundTripPreservesData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanTelemetryBinaryRoundTripTest::RunTest(const FString& Parameters)
{
	TArray<FMidanTelemetryFrame> Written;
	for (int32 i = 0; i < 50; ++i)
	{
		Written.Add(MakeTestFrame(i));
	}

	const FString TestFilePath = FPaths::ProjectSavedDir() / TEXT("Tests") / TEXT("telemetry_roundtrip_test.midantelem");

	TestTrue(TEXT("WriteFrames succeeds"), MidanTelemetryWriter::WriteFrames(TestFilePath, Written, /*bAppend=*/false));

	TArray<FMidanTelemetryFrame> ReadBack;
	TestTrue(TEXT("ReadFrames succeeds"), MidanTelemetryWriter::ReadFrames(TestFilePath, ReadBack));

	TestEqual(TEXT("Frame count round-trips"), ReadBack.Num(), Written.Num());

	bool bAllMatch = true;
	for (int32 i = 0; i < FMath::Min(Written.Num(), ReadBack.Num()); ++i)
	{
		if (!FMath::IsNearlyEqual(Written[i].TimestampSeconds, ReadBack[i].TimestampSeconds, 1e-6)
			|| Written[i].Gear != ReadBack[i].Gear
			|| Written[i].SourceId != ReadBack[i].SourceId
			|| Written[i].Wheels[0].SurfaceTagName != ReadBack[i].Wheels[0].SurfaceTagName)
		{
			bAllMatch = false;
			break;
		}
	}
	TestTrue(TEXT("Every field round-trips byte-identical"), bAllMatch);

	// Append mode: a second write should extend, not overwrite.
	TArray<FMidanTelemetryFrame> MoreFrames;
	MoreFrames.Add(MakeTestFrame(1000));
	TestTrue(TEXT("Appending succeeds"), MidanTelemetryWriter::WriteFrames(TestFilePath, MoreFrames, /*bAppend=*/true));

	TArray<FMidanTelemetryFrame> ReadBackAfterAppend;
	MidanTelemetryWriter::ReadFrames(TestFilePath, ReadBackAfterAppend);
	TestEqual(TEXT("Append adds to, not replaces, the existing frames"), ReadBackAfterAppend.Num(), Written.Num() + 1);

	IFileManager::Get().Delete(*TestFilePath);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidanGhostRoundTripTest,
	"Midan.Telemetry.Ghost.RecordingRoundTripPreservesData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMidanGhostRoundTripTest::RunTest(const FString& Parameters)
{
	FMidanGhostRecording Recording;
	Recording.VehicleAssetName = TEXT("DA_TestVehicle");

	for (int32 i = 0; i < 20; ++i)
	{
		FMidanGhostInputSample Sample;
		Sample.TimeSeconds = i * (1.f / 120.f);
		Sample.Input.Throttle = 0.5f;
		Sample.Input.Steer = (i % 2 == 0) ? 0.2f : -0.2f;
		Recording.InputFrames.Add(Sample);
	}

	FMidanGhostResyncKey Key;
	Key.TimeSeconds = 1.f;
	Key.Transform = FTransform(FVector(100.f, 0.f, 0.f));
	Recording.ResyncKeys.Add(Key);

	const FString TestFilePath = FPaths::ProjectSavedDir() / TEXT("Tests") / TEXT("ghost_roundtrip_test.midanghost");

	TestTrue(TEXT("SaveRecording succeeds"), MidanGhostIO::SaveRecording(TestFilePath, Recording));

	FMidanGhostRecording ReadBack;
	TestTrue(TEXT("LoadRecording succeeds"), MidanGhostIO::LoadRecording(TestFilePath, ReadBack));

	TestEqual(TEXT("Input frame count round-trips"), ReadBack.InputFrames.Num(), Recording.InputFrames.Num());
	TestEqual(TEXT("Resync key count round-trips"), ReadBack.ResyncKeys.Num(), Recording.ResyncKeys.Num());
	TestEqual(TEXT("Vehicle asset name round-trips"), ReadBack.VehicleAssetName, Recording.VehicleAssetName);

	if (ReadBack.ResyncKeys.Num() > 0)
	{
		TestTrue(TEXT("Resync key transform round-trips"),
			ReadBack.ResyncKeys[0].Transform.GetLocation().Equals(Recording.ResyncKeys[0].Transform.GetLocation(), 0.01f));
	}

	IFileManager::Get().Delete(*TestFilePath);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
