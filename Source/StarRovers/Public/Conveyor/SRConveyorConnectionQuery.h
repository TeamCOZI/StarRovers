#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"

class USRPlanetSurfaceGrid;

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorConnectionQuery
	{
		static bool TryResolveLaneByDirection(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRConveyorSegment& Segment,
			ESRConveyorGridDirection Direction,
			FSRConveyorLaneKey& OutLaneKey);

		static bool DoesSegmentReferenceLane(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRConveyorSegment& Segment,
			const FSRConveyorLaneKey& TargetLaneKey);

		static bool GatherConnectedLaneKeysAtCell(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const FSRPlanetSurfaceGridCellId& CellId,
			int32 Layer,
			TArray<FSRConveyorLaneKey>& OutLaneKeys);
	};
}
