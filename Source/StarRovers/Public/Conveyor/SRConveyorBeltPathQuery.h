#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"

class USRPlanetSurfaceGrid;

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorBeltPathQuery
	{
		static bool GatherConnectedCellIds(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const FSRPlanetSurfaceGridCellId& CellId,
			int32 Layer,
			TArray<FSRPlanetSurfaceGridCellId>& OutCellIds);

		static bool GatherConnectedBeltPaths(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const TArray<FSRConveyorBeltPath>& BeltPaths,
			const FSRPlanetSurfaceGridCellId& CellId,
			int32 Layer,
			TArray<FSRConveyorBeltPath>& OutBeltPaths);

		static bool GatherBeltPathsInCells(
			const TArray<FSRConveyorBeltPath>& BeltPaths,
			const TSet<FSRPlanetSurfaceGridCellId>& CellIds,
			TArray<FSRConveyorBeltPath>& OutBeltPaths);
	};
}
