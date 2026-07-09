#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorNetworkRuntimeState.h"
#include "Conveyor/SRConveyorRemovalPlanner.h"
#include "Templates/Function.h"

class USRPlanetSurfaceGrid;
class USRStructureDataAsset;

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorMutationFinalizer
	{
		static void RemoveTransportItems(
			FSRConveyorTransportRuntimeState& TransportState,
			const FSRConveyorRemovalResult& RemovalResult);

		static void ClearSurfaceCells(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TArray<FSRPlanetSurfaceGridCellId>& CellIds);

		static void MarkPlacementActorGroup(
			FSRConveyorActorGroupRuntimeState& ActorGroupState,
			USRStructureDataAsset* StructureDataAsset,
			int32 Layer);

		static void MarkDeletionActorGroups(
			FSRConveyorActorGroupRuntimeState& ActorGroupState,
			const FSRConveyorRemovalResult& RemovalResult);

		static void LogDeletionDestroyedActorGroups(
			const FSRConveyorRemovalResult& RemovalResult,
			const TCHAR* Label,
			TFunctionRef<void(const TCHAR* LogLabel, FName ActorGroupKey, bool bRequestGarbageCollection)> LogMutationDiagnostics);
	};
}
