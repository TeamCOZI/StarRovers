#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Conveyor/SRConveyorNetworkGeometry.h"
#include "Conveyor/SRConveyorPathfinder.h"
#include "Conveyor/SRConveyorPlacementValidator.h"

bool USRConveyorNetworkComponent::FindConveyorPath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& StartCellId,
	const FSRPlanetSurfaceGridCellId& EndCellId,
	int32 Layer,
	TArray<FSRPlanetSurfaceGridCellId>& OutPath) const
{
	const TSet<FSRPlanetSurfaceGridCellId> EmptyBlockedCellIds;
	return FindConveyorPathAvoidingCells(
		SurfaceGrid,
		StartCellId,
		EndCellId,
		Layer,
		EmptyBlockedCellIds,
		OutPath);
}

bool USRConveyorNetworkComponent::FindConveyorPathAvoidingCells(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& StartCellId,
	const FSRPlanetSurfaceGridCellId& EndCellId,
	int32 Layer,
	const TSet<FSRPlanetSurfaceGridCellId>& AdditionalBlockedCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& OutPath) const
{
	OutPath.Reset();

	const int32 SafeLayer = FMath::Max(0, Layer);
	if (!(StartCellId == EndCellId) && AdditionalBlockedCellIds.Contains(EndCellId))
	{
		return false;
	}

	const auto CanUseCellForPathSearch = [this, SurfaceGrid, SafeLayer, StartCellId, EndCellId, &AdditionalBlockedCellIds](const FSRPlanetSurfaceGridCellId& CellId)
	{
		if (AdditionalBlockedCellIds.Contains(CellId))
		{
			return CellId == StartCellId;
		}

		const FSRConveyorLaneKey LaneKey = StarRovers::Conveyor::FSRConveyorNetworkGeometry::MakeLaneKey(CellId, SafeLayer);
		if (Segments.Contains(LaneKey))
		{
			return CellId == StartCellId || CellId == EndCellId;
		}

		return StarRovers::Conveyor::FSRConveyorPlacementValidator::CanPlaceNewLane(SurfaceGrid, Segments, LaneKey);
	};

	return StarRovers::Conveyor::FSRConveyorPathfinder::FindPath(
		SurfaceGrid,
		StartCellId,
		EndCellId,
		CanUseCellForPathSearch,
		OutPath);
}
