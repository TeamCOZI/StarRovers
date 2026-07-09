#include "Conveyor/SRConveyorBeltPathQuery.h"

#include "Conveyor/SRConveyorConnectionQuery.h"
#include "Conveyor/SRConveyorNetworkGeometry.h"
#include "Conveyor/SRConveyorBeltPathSplitter.h"

bool StarRovers::Conveyor::FSRConveyorBeltPathQuery::GatherConnectedCellIds(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer,
	TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
{
	OutCellIds.Reset();

	TArray<FSRConveyorLaneKey> ConnectedLaneKeys;
	if (!FSRConveyorConnectionQuery::GatherConnectedLaneKeysAtCell(SurfaceGrid, Segments, CellId, Layer, ConnectedLaneKeys))
	{
		return false;
	}

	OutCellIds.Reserve(ConnectedLaneKeys.Num());
	for (const FSRConveyorLaneKey& LaneKey : ConnectedLaneKeys)
	{
		OutCellIds.AddUnique(LaneKey.CellId);
	}
	return !OutCellIds.IsEmpty();
}

bool StarRovers::Conveyor::FSRConveyorBeltPathQuery::GatherConnectedBeltPaths(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const TArray<FSRConveyorBeltPath>& BeltPaths,
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer,
	TArray<FSRConveyorBeltPath>& OutBeltPaths)
{
	OutBeltPaths.Reset();

	TArray<FSRConveyorLaneKey> ConnectedLaneKeys;
	if (!FSRConveyorConnectionQuery::GatherConnectedLaneKeysAtCell(SurfaceGrid, Segments, CellId, Layer, ConnectedLaneKeys))
	{
		return false;
	}

	TSet<FSRConveyorLaneKey> ConnectedLaneKeySet;
	ConnectedLaneKeySet.Reserve(ConnectedLaneKeys.Num());
	for (const FSRConveyorLaneKey& LaneKey : ConnectedLaneKeys)
	{
		ConnectedLaneKeySet.Add(LaneKey);
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	for (const FSRConveyorBeltPath& BeltPath : BeltPaths)
	{
		if (BeltPath.Layer != SafeLayer)
		{
			continue;
		}

		FSRConveyorBeltPathSplitter::AppendMatchingSubPaths(
			BeltPath,
			[&ConnectedLaneKeySet, SafeLayer](const FSRPlanetSurfaceGridCellId& PathCellId)
		{
			return ConnectedLaneKeySet.Contains(FSRConveyorNetworkGeometry::MakeLaneKey(PathCellId, SafeLayer));
		},
			OutBeltPaths);
	}

	return !OutBeltPaths.IsEmpty();
}

bool StarRovers::Conveyor::FSRConveyorBeltPathQuery::GatherBeltPathsInCells(
	const TArray<FSRConveyorBeltPath>& BeltPaths,
	const TSet<FSRPlanetSurfaceGridCellId>& CellIds,
	TArray<FSRConveyorBeltPath>& OutBeltPaths)
{
	OutBeltPaths.Reset();
	if (CellIds.IsEmpty())
	{
		return false;
	}

	for (const FSRConveyorBeltPath& BeltPath : BeltPaths)
	{
		FSRConveyorBeltPathSplitter::AppendMatchingSubPaths(
			BeltPath,
			[&CellIds](const FSRPlanetSurfaceGridCellId& PathCellId)
		{
			return CellIds.Contains(PathCellId);
		},
			OutBeltPaths);
	}

	return !OutBeltPaths.IsEmpty();
}
