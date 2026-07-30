"""Validate every Midan tuning Data Asset from the Unreal editor.

Run inside the editor (Output Log -> Python), or headless:

    <UE>/Engine/Binaries/Mac/UnrealEditor-Cmd \\
        <abs>/Midan.uproject \\
        -run=pythonscript -script="<abs>/Tools/editor_python/validate_vehicle_data.py"

Exits non-zero when any asset has a validation ERROR, so CI can gate on it.
Warnings are reported but never fail the run — a high centre of mass on the
rally car is intentional, and a validator that fights the design gets disabled.

Requires the Python Editor Script Plugin (docs/MANUAL_STEPS.md 0.2).

This script deliberately contains NO validation rules. Every rule lives in C++
in ValidateMidanData, so the editor, this script, CI, and the Automation Specs
all check the identical contract. A rule duplicated here would drift from the
C++ one and the two would silently disagree.
"""

import sys
import unreal


# Primary asset types registered by GetPrimaryAssetId on each asset class.
ASSET_TYPES = (
    "MidanVehicle",
    "MidanVehicleFeel",
    "MidanSurfaceResponse",
)

CONTENT_ROOT = "/Game/Midan"


def find_midan_data_assets():
    """Return every UMidanDataAsset under CONTENT_ROOT.

    Searched by base class rather than by the primary-asset-type registry, so a
    newly added asset type is picked up without touching this script — the same
    reason there is one C++ validator rather than one per type.
    """
    registry = unreal.AssetRegistryHelpers.get_asset_registry()

    search = unreal.ARFilter(
        class_paths=[unreal.TopLevelAssetPath("/Script/MidanCore", "MidanDataAsset")],
        package_paths=[CONTENT_ROOT],
        recursive_paths=True,
        recursive_classes=True,
    )

    return registry.get_assets(search)


def validate_asset(asset_data):
    """Validate one asset. Returns (error_count, warning_count)."""
    asset = asset_data.get_asset()
    if asset is None:
        unreal.log_error(f"Could not load asset: {asset_data.package_name}")
        return (1, 0)

    name = asset.get_name()

    # ValidateMidanData is not exposed to Python (FMidanValidationResult is not
    # a BlueprintType output parameter), so route through the editor's Data
    # Validation framework — which UMidanDataAssetValidator hooks into. That
    # keeps one code path rather than a Python-only shortcut that could diverge.
    subsystem = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    if subsystem is None:
        unreal.log_error("EditorValidatorSubsystem unavailable. Is the DataValidation plugin enabled?")
        return (1, 0)

    settings = unreal.ValidateAssetsSettings()
    settings.validate_referencing_assets = False
    settings.show_if_no_failures = False

    results = subsystem.validate_assets_with_settings([asset_data], settings)

    errors = int(getattr(results, "invalid", 0) or 0)
    warnings = int(getattr(results, "warnings", 0) or 0)

    if errors:
        unreal.log_error(f"  FAIL  {name}")
    elif warnings:
        unreal.log_warning(f"  WARN  {name}  ({warnings} warning(s))")
    else:
        unreal.log(f"  PASS  {name}")

    return (errors, warnings)


def main():
    unreal.log("")
    unreal.log("MIDAN - DATA ASSET VALIDATION")
    unreal.log(f"Searching {CONTENT_ROOT} for UMidanDataAsset subclasses")
    unreal.log("")

    assets = find_midan_data_assets()

    if not assets:
        # Not a failure. Before Phase 3 there is no content yet, and a script
        # that fails on an empty project would block CI for the wrong reason.
        unreal.log_warning(
            f"No Midan data assets found under {CONTENT_ROOT}. "
            "Expected until vehicle assets are authored - see docs/MANUAL_STEPS.md."
        )
        return 0

    total_errors = 0
    total_warnings = 0

    for asset_data in assets:
        errors, warnings = validate_asset(asset_data)
        total_errors += errors
        total_warnings += warnings

    unreal.log("")
    unreal.log(
        f"{len(assets)} asset(s) checked - "
        f"{total_errors} error(s), {total_warnings} warning(s)"
    )

    if total_errors:
        unreal.log_error(
            f"VALIDATION FAILED: {total_errors} error(s). "
            "See the Output Log above for the failing field on each asset."
        )
        return 1

    unreal.log("VALIDATION PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
