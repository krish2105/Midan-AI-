// MidanEditorTools module implementation.
//
// Responsibility: module lifetime and, from Phase 2, registration of the
// asset validators that route UMidanDataAsset::ValidateData into the editor
// Data Validation framework so CI can run -run=DataValidation.
// Single reason to change: the set of registered editor extensions changes.

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, MidanEditorTools);
