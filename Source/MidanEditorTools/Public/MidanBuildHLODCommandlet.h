// Scripted HLOD level 0 and 1 build for the World Partition circuit map.
//
// Responsibility: build both HLOD levels in one command, for CI and for a
// human who does not want to remember the builder invocation by hand.
// Single reason to change: how HLOD levels are built for this project
// changes.
//
// A thin wrapper over UWorldPartitionHLODsBuilder, run twice (level 0, then
// level 1) — the algorithm is Epic's own World Partition HLOD pipeline, not
// something this project reimplements. Wrapping it in one commandlet exists
// so `Tools/build/build.py` (Phase 10) has a single command to shell out to
// rather than needing to know the two-pass invocation itself.
//
// API VERIFY: UWorldPartitionHLODsBuilder's exact API (constructor
// arguments, the run/build entry point name) is unconfirmed without an
// installed 5.8 engine — see docs/ASSUMPTIONS.md.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MidanBuildHLODCommandlet.generated.h"

UCLASS()
class UMidanBuildHLODCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMidanBuildHLODCommandlet();

	//~ UCommandlet
	virtual int32 Main(const FString& Params) override;

private:
	/** Runs UWorldPartitionHLODsBuilder for one HLOD level against MapPath.
	 *  Returns true on success. */
	bool BuildHLODLevel(const FString& MapPath, int32 HLODLevel) const;
};
