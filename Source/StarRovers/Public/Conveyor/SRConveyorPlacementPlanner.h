#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"

class USRPlanetSurfaceGrid;
class USRStructureDataAsset;

namespace StarRovers::Conveyor
{
	struct FSRConveyorPlacementPlan
	{
		int32 Layer = 0;
		int32 PreviousBeltPathCount = 0;
		TArray<FSRConveyorSegment> ProposedSegments;
		TMap<FSRConveyorLaneKey, FSRConveyorSegment> PreviousSegments;
		TArray<FSRPlanetSurfaceGridCellId> DestructibleStructureCellIds;
		FSRConveyorBeltPath BeltPath;
	};

	struct STARROVERS_API FSRConveyorPlacementPlanner
	{
		static bool CanPlacePath(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
			int32 Layer,
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const TSet<FSRPlanetSurfaceGridCellId>& IgnoredOccupiedCellIds);

		static bool BuildPlacementPlan(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
			int32 Layer,
			float LayerHeight,
			float DefaultLayerHeight,
			USRStructureDataAsset* StructureDataAsset,
			FName NetworkId,
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			int32 CurrentBeltPathCount,
			FSRConveyorPlacementPlan& OutPlan);

		static void ApplyPlacementPlan(
			const FSRConveyorPlacementPlan& Plan,
			TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			TArray<FSRConveyorBeltPath>& BeltPaths);

		static void RollbackPlacementPlan(
			const FSRConveyorPlacementPlan& Plan,
			TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			TArray<FSRConveyorBeltPath>& BeltPaths);
	};
}
