#include "MidanInputConfigDataAsset.h"

#define LOCTEXT_NAMESPACE "MidanInputConfig"

void UMidanInputConfigDataAsset::ValidateMidanData(FMidanValidationResult& Result) const
{
	Super::ValidateMidanData(Result);

	// Every driving action must be assigned. A null action is not a soft
	// failure: Enhanced Input silently binds nothing, so the car simply does not
	// respond and the cause is invisible at runtime. Catching it here turns a
	// baffling playtest into a validation error.
	struct FRequiredAction
	{
		const TSoftObjectPtr<UInputAction>& Action;
		const TCHAR* Name;
	};

	const FRequiredAction RequiredActions[] = {
		{ ThrottleAction,  TEXT("ThrottleAction")  },
		{ BrakeAction,     TEXT("BrakeAction")     },
		{ SteerAction,     TEXT("SteerAction")     },
		{ HandbrakeAction, TEXT("HandbrakeAction") },
		{ ShiftUpAction,   TEXT("ShiftUpAction")   },
		{ ShiftDownAction, TEXT("ShiftDownAction") },
	};

	for (const FRequiredAction& Required : RequiredActions)
	{
		if (Required.Action.IsNull())
		{
			Result.AddError(
				FName(Required.Name),
				FText::Format(
					LOCTEXT("MissingAction",
						"{0} is unassigned. Enhanced Input binds nothing for a null action, so the "
						"vehicle will not respond and the cause will not be visible at runtime. "
						"Author the asset per docs/MANUAL_STEPS.md 1.3."),
					FText::FromString(Required.Name)));
		}
	}

	if (DrivingContext.IsNull())
	{
		Result.AddError(TEXT("DrivingContext"),
			LOCTEXT("MissingContext",
				"DrivingContext is unassigned. Without a mapping context no action ever fires, "
				"regardless of whether the actions themselves are assigned."));
	}

	// Optional actions warn rather than error — a Phase 3 test setup legitimately
	// has no pause menu or camera modes yet.
	if (CameraModeAction.IsNull())
	{
		Result.AddWarning(TEXT("CameraModeAction"),
			LOCTEXT("MissingCameraMode",
				"CameraModeAction is unassigned. The four camera modes (Phase 4) will not be "
				"selectable. Expected before the Phase 4 gate."));
	}

	if (RespawnAction.IsNull())
	{
		Result.AddWarning(TEXT("RespawnAction"),
			LOCTEXT("MissingRespawn",
				"RespawnAction is unassigned. Checkpoint respawn (Phase 5) will be unreachable."));
	}

	if (PauseAction.IsNull())
	{
		Result.AddWarning(TEXT("PauseAction"),
			LOCTEXT("MissingPause",
				"PauseAction is unassigned. The pause menu (Phase 7) will be unreachable."));
	}
}

#undef LOCTEXT_NAMESPACE
