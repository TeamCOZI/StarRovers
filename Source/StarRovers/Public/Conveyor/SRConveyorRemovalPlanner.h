#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"

class USRPlanetSurfaceGrid;
class USRStructureDataAsset;

namespace StarRovers::Conveyor
{
	struct FSRConveyorRemovalResult
	{
		int32 Layer = 0;
		TArray<FSRConveyorLaneKey> RemovedLaneKeys;
		TArray<FSRPlanetSurfaceGridCellId> ClearedCellIds;
		TArray<USRStructureDataAsset*> AffectedStructureDataAssets;
	};

	struct STARROVERS_API FSRConveyorRemovalPlanner
	{
		static bool RemoveConveyorAtCell(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRPlanetSurfaceGridCellId& CellId,
			int32 Layer,
			TArray<FSRConveyorBeltPath>& BeltPaths,
			TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			FSRConveyorRemovalResult& OutResult);

		static bool RemoveBeltPath(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRConveyorBeltPath& BeltPath,
			const TArray<FSRPlanetSurfaceGridCellId>& PlacedCellIds,
			TArray<FSRConveyorBeltPath>& BeltPaths,
			TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			FSRConveyorRemovalResult& OutResult);

		static bool RemoveLaneKeys(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TArray<FSRConveyorLaneKey>& LaneKeys,
			int32 Layer,
			TArray<FSRConveyorBeltPath>& BeltPaths,
			TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			FSRConveyorRemovalResult& OutResult);

		static void CollectLaneKeysInCells(
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const TSet<FSRPlanetSurfaceGridCellId>& CellIds,
			TArray<FSRConveyorLaneKey>& OutLaneKeys);
	};
}
