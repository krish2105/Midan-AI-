// MidanCore module implementation.
//
// Responsibility: module lifetime for the primary game module.
// Single reason to change: module startup ordering changes.
//
// Log categories are declared in MidanLogChannels.h and defined here, so any
// module can log without owning a category.

#include "Modules/ModuleManager.h"
#include "MidanLogChannels.h"

DEFINE_LOG_CATEGORY(LogMidanCore);
DEFINE_LOG_CATEGORY(LogMidanVehicle);
DEFINE_LOG_CATEGORY(LogMidanRace);
DEFINE_LOG_CATEGORY(LogMidanAI);
DEFINE_LOG_CATEGORY(LogMidanTelemetry);

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, MidanCore, "Midan");
