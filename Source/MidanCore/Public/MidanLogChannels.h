// Log categories, declared centrally so any module can log without owning one.
//
// Responsibility: the project's logging vocabulary.
// Single reason to change: a module is added or removed.
//
// Pulled forward from Phase 3 to Phase 2 (docs/ASSUMPTIONS.md A20):
// UVehicleSetupApplier must report validation failures at apply time, because a
// setup that fails validation would otherwise produce NaN in the solver and
// surface as the car silently vanishing.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

MIDANCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogMidanCore, Log, All);
MIDANCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogMidanVehicle, Log, All);
MIDANCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogMidanRace, Log, All);
MIDANCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogMidanAI, Log, All);
MIDANCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogMidanTelemetry, Log, All);
