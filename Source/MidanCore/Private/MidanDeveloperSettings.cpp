#include "MidanDeveloperSettings.h"

UMidanDeveloperSettings::UMidanDeveloperSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Midan");

	// No asset defaults are assigned here. Assigning a TSoftObjectPtr in a
	// constructor would require naming a content path that does not exist until
	// Phase 3 authoring, and CLAUDE.md forbids resolving assets in constructors
	// regardless. These are set in the project settings UI or DefaultGame.ini.
}

const UMidanDeveloperSettings* UMidanDeveloperSettings::Get()
{
	// GetDefault on a Config UDeveloperSettings returns the CDO with ini values
	// already applied. Cheap enough to call per frame, but callers in the
	// physics callback should cache it at BeginPlay rather than fetch per
	// substep.
	return GetDefault<UMidanDeveloperSettings>();
}

bool UMidanDeveloperSettings::IsAeroDebugDrawEnabled() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return bDebugDrawAeroForces;
#endif
}

bool UMidanDeveloperSettings::IsWheelSurfaceDebugDrawEnabled() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return bDebugDrawWheelSurfaces;
#endif
}
