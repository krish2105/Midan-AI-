// Resolves cross-module interfaces without a module hard-linking a sibling.
//
// Responsibility: register and resolve the project's cross-module interfaces.
// Single reason to change: a new interface crosses a module boundary.
//
// This is the mechanism that keeps the dependency graph in
// docs/ARCHITECTURE.md §2.1 literal. MidanRace needs vehicle state but does not
// depend on MidanVehicle; MidanAI needs race phase but does not depend on
// MidanRace. Providers register themselves on BeginPlay and consumers resolve
// by interface. See docs/ASSUMPTIONS.md A7-A10 for why each case is routed here
// rather than solved by widening the graph.
//
// Cost: one indirection per cross-module read, plus registration order to get
// right. Benefit: every module compiles and unit-tests standalone.
//
// This is NOT a general service registry and must not become one. It holds a
// fixed, small set of named interfaces. If it grows an AddService(FName)
// method, the dependency graph has stopped being a design and become a
// suggestion.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ScriptInterface.h"
#include "MidanServiceLocator.generated.h"

class IMidanTrackInterface;
class IMidanRaceStateInterface;
class IMidanTelemetrySink;

UCLASS()
class MIDANCORE_API UMidanServiceLocatorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Track — provided by AMidanTrackSpline (MidanRace, Phase 5).
	//  Consumed by MidanAI for curvature lookahead and by MidanRace for position.

	/** Registers the track. Warns and overwrites if one is already registered:
	 *  two tracks in a level is an authoring error, and silently keeping the
	 *  first would make the symptom appear far from the cause. */
	void RegisterTrack(const TScriptInterface<IMidanTrackInterface>& InTrack);
	void UnregisterTrack();

	/** May return an invalid interface — a level without a track is legitimate
	 *  during Phase 3 vehicle testing. Callers must check. */
	TScriptInterface<IMidanTrackInterface> GetTrack() const { return Track; }
	bool HasTrack() const { return Track.GetObject() != nullptr; }

	//~ Race state — provided by AMidanRaceGameState (MidanRace, Phase 5).
	//  Consumed by MidanAI so an opponent does not drive during the countdown.

	void RegisterRaceState(const TScriptInterface<IMidanRaceStateInterface>& InRaceState);
	void UnregisterRaceState();
	TScriptInterface<IMidanRaceStateInterface> GetRaceState() const { return RaceState; }
	bool HasRaceState() const { return RaceState.GetObject() != nullptr; }

	//~ Telemetry sink — provided by UMidanTelemetrySubsystem (MidanTelemetry, Phase 9).
	//  Consumed by MidanAI to log every rubber-band application (master prompt §4.4).

	void RegisterTelemetrySink(const TScriptInterface<IMidanTelemetrySink>& InSink);
	void UnregisterTelemetrySink();
	TScriptInterface<IMidanTelemetrySink> GetTelemetrySink() const { return TelemetrySink; }

	/**
	 * True when telemetry is present AND capturing.
	 *
	 * Call this before doing any work to build a telemetry event. Without it,
	 * every rubber-band evaluation formats a payload that gets discarded, which
	 * is wasted work on the game thread in a Shipping build where capture is
	 * usually off.
	 */
	bool IsTelemetryCapturing() const;

	//~ UWorldSubsystem
	virtual void Deinitialize() override;

private:
	UPROPERTY(Transient)
	TScriptInterface<IMidanTrackInterface> Track;

	UPROPERTY(Transient)
	TScriptInterface<IMidanRaceStateInterface> RaceState;

	UPROPERTY(Transient)
	TScriptInterface<IMidanTelemetrySink> TelemetrySink;
};
