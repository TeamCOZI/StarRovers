#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorSegmentQuery
	{
		static bool HasSegmentAtCell(
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const FSRPlanetSurfaceGridCellId& CellId);

		static void GatherCellIdsAtLayer(
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			int32 Layer,
			TArray<FSRPlanetSurfaceGridCellId>& OutCellIds);
	};
}
