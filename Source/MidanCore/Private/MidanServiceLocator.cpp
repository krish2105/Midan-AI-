#include "MidanServiceLocator.h"

#include "MidanLogChannels.h"
#include "MidanRaceStateInterface.h"
#include "MidanTelemetryInterfaces.h"
#include "MidanTrackInterface.h"

namespace
{
	/**
	 * Shared registration guard.
	 *
	 * Two providers of the same interface in one level is an authoring error.
	 * Overwriting and warning is the right response: silently keeping the first
	 * makes the symptom (the AI following the wrong spline) appear a long way
	 * from the cause (a duplicated actor).
	 */
	template <typename TInterface>
	void RegisterProvider(
		TScriptInterface<TInterface>& Slot,
		const TScriptInterface<TInterface>& Incoming,
		const TCHAR* ProviderName)
	{
		if (Incoming.GetObject() == nullptr)
		{
			UE_LOG(LogMidanCore, Warning,
				TEXT("Service locator: refused to register a null %s provider."), ProviderName);
			return;
		}

		if (Slot.GetObject() != nullptr && Slot.GetObject() != Incoming.GetObject())
		{
			UE_LOG(LogMidanCore, Warning,
				TEXT("Service locator: %s already registered by '%s'; overwriting with '%s'. ")
				TEXT("Two providers in one level is an authoring error — check for a duplicated actor."),
				ProviderName,
				*GetNameSafe(Slot.GetObject()),
				*GetNameSafe(Incoming.GetObject()));
		}

		Slot = Incoming;

		UE_LOG(LogMidanCore, Verbose, TEXT("Service locator: registered %s provider '%s'."),
			ProviderName, *GetNameSafe(Incoming.GetObject()));
	}

	/** Only clears the slot if the caller is the current provider. A provider
	 *  torn down after its replacement registered must not clear the new one —
	 *  actor destruction order is not guaranteed. */
	template <typename TInterface>
	void UnregisterProvider(TScriptInterface<TInterface>& Slot, const TCHAR* ProviderName)
	{
		if (Slot.GetObject() == nullptr)
		{
			return;
		}

		UE_LOG(LogMidanCore, Verbose, TEXT("Service locator: unregistered %s provider '%s'."),
			ProviderName, *GetNameSafe(Slot.GetObject()));

		Slot = nullptr;
	}
}

void UMidanServiceLocatorSubsystem::RegisterTrack(const TScriptInterface<IMidanTrackInterface>& InTrack)
{
	RegisterProvider(Track, InTrack, TEXT("Track"));
}

void UMidanServiceLocatorSubsystem::UnregisterTrack()
{
	UnregisterProvider(Track, TEXT("Track"));
}

void UMidanServiceLocatorSubsystem::RegisterRaceState(const TScriptInterface<IMidanRaceStateInterface>& InRaceState)
{
	RegisterProvider(RaceState, InRaceState, TEXT("RaceState"));
}

void UMidanServiceLocatorSubsystem::UnregisterRaceState()
{
	UnregisterProvider(RaceState, TEXT("RaceState"));
}

void UMidanServiceLocatorSubsystem::RegisterTelemetrySink(const TScriptInterface<IMidanTelemetrySink>& InSink)
{
	RegisterProvider(TelemetrySink, InSink, TEXT("TelemetrySink"));
}

void UMidanServiceLocatorSubsystem::UnregisterTelemetrySink()
{
	UnregisterProvider(TelemetrySink, TEXT("TelemetrySink"));
}

bool UMidanServiceLocatorSubsystem::IsTelemetryCapturing() const
{
	// Both checks matter. A registered sink that is not capturing is the normal
	// Shipping case, and callers use this to skip building an event payload.
	const IMidanTelemetrySink* Sink = TelemetrySink.GetInterface();
	return Sink != nullptr && Sink->IsCapturing();
}

void UMidanServiceLocatorSubsystem::Deinitialize()
{
	// Clear explicitly rather than relying on GC. These are interface handles to
	// actors in a world that is tearing down; holding them past Deinitialize
	// means a late resolve can hand out a pointer to a half-destroyed actor.
	Track = nullptr;
	RaceState = nullptr;
	TelemetrySink = nullptr;

	Super::Deinitialize();
}
