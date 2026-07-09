#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

class USRPlanetSurfaceGrid;

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorNetworkGeometry
	{
		static FSRConveyorLaneKey MakeLaneKey(const FSRPlanetSurfaceGridCellId& CellId, int32 Layer);
		static void SortLaneKeys(TArray<FSRConveyorLaneKey>& LaneKeys);
		static ESRConveyorGridDirection GetOppositeDirection(ESRConveyorGridDirection Direction);
		static ESRConveyorSegmentShape ResolveSegmentShape(ESRConveyorGridDirection InputDirection, ESRConveyorGridDirection OutputDirection);
		static bool GetNeighborCellIdByDirection(const FSRPlanetSurfaceGridCellNeighbors& Neighbors, ESRConveyorGridDirection Direction, FSRPlanetSurfaceGridCellId& OutCellId);
		static bool FindDirectionBetweenCells(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& FromCellId, const FSRPlanetSurfaceGridCellId& ToCellId, ESRConveyorGridDirection& OutDirection);
		static int32 GetDirectionClockwiseOrder(ESRConveyorGridDirection Direction);
		static void SortDirectionsClockwise(TArray<ESRConveyorGridDirection>& Directions);
		static void CollectInputDirections(const FSRConveyorSegment& Segment, TArray<ESRConveyorGridDirection>& OutDirections);
		static void CollectOutputDirections(const FSRConveyorSegment& Segment, TArray<ESRConveyorGridDirection>& OutDirections);
	};
}
