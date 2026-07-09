#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Utility/SRMemoryDiagnostics.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSRMemoryDiagnosticsConveyorDelete(
		TEXT("sr.MemoryDiagnostics.ConveyorDelete"),
		0,
		TEXT("Logs memory diagnostics after conveyor deletion refreshes. 0=disabled, 1=enabled."));

	TAutoConsoleVariable<int32> CVarSRMemoryDiagnosticsConveyorPlacement(
		TEXT("sr.MemoryDiagnostics.ConveyorPlacement"),
		0,
		TEXT("Logs memory diagnostics after conveyor placement refreshes. 0=disabled, 1=enabled."));

	TAutoConsoleVariable<int32> CVarSRMemoryDiagnosticsForceGCOnConveyorDelete(
		TEXT("sr.MemoryDiagnostics.ForceGCOnConveyorDelete"),
		0,
		TEXT("Requests garbage collection after conveyor deletion diagnostics. 0=log next tick only, 1=force GC and log AfterGCTick."));
}

void USRConveyorNetworkComponent::LogConveyorMutationMemoryDiagnostics(const TCHAR* Label, FName ActorGroupKey, bool bRequestGarbageCollection) const
{
	const FString LabelString(Label);
	const bool bIsPlacementLog = LabelString.StartsWith(TEXT("ConveyorPlace"));
	const bool bIsDeletionLog = LabelString.StartsWith(TEXT("ConveyorDelete"));
	if ((bIsPlacementLog && CVarSRMemoryDiagnosticsConveyorPlacement.GetValueOnAnyThread() == 0)
		|| (bIsDeletionLog && CVarSRMemoryDiagnosticsConveyorDelete.GetValueOnAnyThread() == 0))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	TArray<FString> ExtraLines;
	ExtraLines.Add(FString::Printf(
		TEXT("ConveyorNetwork Owner=%s ActorGroup=%s Segments=%d BeltPaths=%d PlacedActors=%d ActorGroups=%d PendingDeletionDiagnostics=%d ForceGC=%s"),
		*GetNameSafe(GetOwner()),
		*ActorGroupKey.ToString(),
		Segments.Num(),
		BeltPaths.Num(),
		PlacedConveyorActors.Num(),
		ActorGroupState.GroupsByKey.Num(),
		ActorGroupState.PendingDeletionDiagnosticKeys.Num(),
		bRequestGarbageCollection ? TEXT("true") : TEXT("false")));

	if (bRequestGarbageCollection)
	{
		FSRMemoryDiagnostics::RequestGarbageCollectionAndLogNextTick(World, Label, ExtraLines);
		return;
	}

	FSRMemoryDiagnostics::LogSnapshot(World, FString::Printf(TEXT("%s.AfterRefresh"), Label), ExtraLines);
	FSRMemoryDiagnostics::LogSnapshotNextTick(World, FString::Printf(TEXT("%s.AfterTick"), Label), ExtraLines);
}
