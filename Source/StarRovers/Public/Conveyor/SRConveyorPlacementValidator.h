#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"

class USRPlanetSurfaceGrid;

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorPlacementValidator
	{
		static bool CanPlaceNewLane(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& ExistingSegments,
			const FSRConveyorLaneKey& LaneKey);

		static bool CanPlaceOrReuseLane(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& ExistingSegments,
			const FSRConveyorLaneKey& LaneKey,
			const TSet<FSRPlanetSurfaceGridCellId>& IgnoredOccupiedCellIds);

		static bool CanDestroyStructureForPlacement(USRPlanetSurfaceGrid* SurfaceGrid, FName OccupantId);
		static bool TryRemoveDestructibleStructuresAtCells(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TArray<FSRPlanetSurfaceGridCellId>& CellIds);
		static float ResolveLayerHeight(
			USRPlanetSurfaceGrid* SurfaceGrid,
			float RequestedLayerHeight,
			float DefaultLayerHeight);
	};
}
