#include "MidanBuildHLODCommandlet.h"

#include "MidanLogChannels.h"
#include "Misc/CommandLine.h"
#include "WorldPartition/HLOD/HLODBuilder.h"
#include "WorldPartition/WorldPartitionHLODsBuilder.h"

UMidanBuildHLODCommandlet::UMidanBuildHLODCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UMidanBuildHLODCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> SwitchParams;
	ParseCommandLine(*Params, Tokens, Switches, SwitchParams);

	const FString* MapPathPtr = SwitchParams.Find(TEXT("Map"));
	if (!MapPathPtr || MapPathPtr->IsEmpty())
	{
		UE_LOG(LogMidanCore, Error, TEXT("MidanBuildHLOD: -Map=<path> is required, e.g. -Map=/Game/Midan/Maps/L_MidanCircuit"));
		return 1;
	}

	UE_LOG(LogMidanCore, Log, TEXT("MidanBuildHLOD: building HLOD levels 0 and 1 for '%s'."), **MapPathPtr);

	bool bSucceeded = true;
	bSucceeded &= BuildHLODLevel(*MapPathPtr, 0);
	bSucceeded &= BuildHLODLevel(*MapPathPtr, 1);

	if (!bSucceeded)
	{
		UE_LOG(LogMidanCore, Error, TEXT("MidanBuildHLOD: one or both HLOD levels failed to build. See errors above."));
		return 1;
	}

	UE_LOG(LogMidanCore, Log, TEXT("MidanBuildHLOD: both HLOD levels built successfully."));
	return 0;
}

bool UMidanBuildHLODCommandlet::BuildHLODLevel(const FString& MapPath, int32 HLODLevel) const
{
	UE_LOG(LogMidanCore, Log, TEXT("MidanBuildHLOD: building HLOD level %d..."), HLODLevel);

	// API VERIFY: UWorldPartitionHLODsBuilder's construction and run entry
	// point are unconfirmed against 5.8 — this project has no engine
	// installed to check the class against (docs/ASSUMPTIONS.md). The
	// documented Epic workflow this wraps is:
	//   UnrealEditor-Cmd <Map> -run=WorldPartitionBuilderCommandlet
	//       -Builder=WorldPartitionHLODsBuilder -HLODLevel=<N> -AllowCommandletRendering
	// If UWorldPartitionHLODsBuilder's programmatic API does not match this
	// call shape once the editor is available, replace this function's body
	// with an FPlatformProcess::CreateProc shelling out to that exact
	// command line instead — the two-pass STRUCTURE (level 0, then level 1)
	// is what this commandlet exists to fix in place, not the mechanism.
	UWorldPartitionHLODsBuilder* Builder = NewObject<UWorldPartitionHLODsBuilder>(GetTransientPackage());
	if (!Builder)
	{
		UE_LOG(LogMidanCore, Error, TEXT("MidanBuildHLOD: failed to construct UWorldPartitionHLODsBuilder."));
		return false;
	}

	Builder->SetHLODLevel(HLODLevel);

	const bool bSuccess = Builder->RunBuilder(MapPath);
	if (!bSuccess)
	{
		UE_LOG(LogMidanCore, Error, TEXT("MidanBuildHLOD: HLOD level %d build reported failure."), HLODLevel);
	}

	return bSuccess;
}
