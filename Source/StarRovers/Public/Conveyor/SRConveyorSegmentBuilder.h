#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"

class USRPlanetSurfaceGrid;

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorSegmentBuilder
	{
		static bool BuildPathSegments(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
			int32 Layer,
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& ExistingSegments,
			TArray<FSRConveyorSegment>& OutSegments,
			FName NetworkId = NAME_None,
			USRStructureDataAsset* StructureDataAsset = nullptr);

		static void RebuildFromBeltPaths(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TArray<FSRConveyorBeltPath>& BeltPaths,
			TMap<FSRConveyorLaneKey, FSRConveyorSegment>& OutSegments);
	};
}
