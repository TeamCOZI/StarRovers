#include "Conveyor/SRConveyorConnectionQuery.h"

#include "Conveyor/SRConveyorNetworkGeometry.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool StarRovers::Conveyor::FSRConveyorConnectionQuery::TryResolveLaneByDirection(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorSegment& Segment,
	ESRConveyorGridDirection Direction,
	FSRConveyorLaneKey& OutLaneKey)
{
	OutLaneKey = FSRConveyorLaneKey();
	if (!IsValid(SurfaceGrid) || Direction == ESRConveyorGridDirection::None)
	{
		return false;
	}

	FSRPlanetSurfaceGridCellNeighbors Neighbors;
	if (!SurfaceGrid->GetCellNeighbors(Segment.Lane.CellId, Neighbors))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellId NextCellId;
	if (!FSRConveyorNetworkGeometry::GetNeighborCellIdByDirection(Neighbors, Direction, NextCellId))
	{
		return false;
	}

	OutLaneKey = FSRConveyorNetworkGeometry::MakeLaneKey(NextCellId, Segment.Lane.Layer);
	return true;
}

bool StarRovers::Conveyor::FSRConveyorConnectionQuery::DoesSegmentReferenceLane(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorSegment& Segment,
	const FSRConveyorLaneKey& TargetLaneKey)
{
	if (!IsValid(SurfaceGrid) || Segment.Lane.Layer != TargetLaneKey.Layer)
	{
		return false;
	}

	TArray<ESRConveyorGridDirection> Directions;
	Directions.Reserve(6);
	FSRConveyorNetworkGeometry::CollectInputDirections(Segment, Directions);

	TArray<ESRConveyorGridDirection> OutputDirections;
	FSRConveyorNetworkGeometry::CollectOutputDirections(Segment, OutputDirections);
	Directions.Append(OutputDirections);

	for (const ESRConveyorGridDirection Direction : Directions)
	{
		FSRConveyorLaneKey NeighborLaneKey;
		if (TryResolveLaneByDirection(SurfaceGrid, Segment, Direction, NeighborLaneKey)
			&& NeighborLaneKey == TargetLaneKey)
		{
			return true;
		}
	}

	return false;
}

bool StarRovers::Conveyor::FSRConveyorConnectionQuery::GatherConnectedLaneKeysAtCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer,
	TArray<FSRConveyorLaneKey>& OutLaneKeys)
{
	OutLaneKeys.Reset();
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	const FSRConveyorLaneKey StartLaneKey = FSRConveyorNetworkGeometry::MakeLaneKey(CellId, Layer);
	if (!Segments.Contains(StartLaneKey))
	{
		return false;
	}

	TSet<FSRConveyorLaneKey> VisitedLaneKeys;
	TArray<FSRConveyorLaneKey> OpenLaneKeys;
	VisitedLaneKeys.Add(StartLaneKey);
	OpenLaneKeys.Add(StartLaneKey);

	const ESRConveyorGridDirection Directions[] =
	{
		ESRConveyorGridDirection::NegativeU,
		ESRConveyorGridDirection::PositiveU,
		ESRConveyorGridDirection::NegativeV,
		ESRConveyorGridDirection::PositiveV,
	};

	for (int32 OpenIndex = 0; OpenIndex < OpenLaneKeys.Num(); ++OpenIndex)
	{
		const FSRConveyorLaneKey CurrentLaneKey = OpenLaneKeys[OpenIndex];
		const FSRConveyorSegment* CurrentSegment = Segments.Find(CurrentLaneKey);
		if (!CurrentSegment)
		{
			continue;
		}

		for (const ESRConveyorGridDirection Direction : Directions)
		{
			FSRConveyorLaneKey NeighborLaneKey;
			if (!TryResolveLaneByDirection(SurfaceGrid, *CurrentSegment, Direction, NeighborLaneKey)
				|| VisitedLaneKeys.Contains(NeighborLaneKey))
			{
				continue;
			}

			const FSRConveyorSegment* NeighborSegment = Segments.Find(NeighborLaneKey);
			if (!NeighborSegment)
			{
				continue;
			}

			if (!DoesSegmentReferenceLane(SurfaceGrid, *CurrentSegment, NeighborLaneKey)
				&& !DoesSegmentReferenceLane(SurfaceGrid, *NeighborSegment, CurrentLaneKey))
			{
				continue;
			}

			VisitedLaneKeys.Add(NeighborLaneKey);
			OpenLaneKeys.Add(NeighborLaneKey);
		}
	}

	OutLaneKeys = OpenLaneKeys;
	FSRConveyorNetworkGeometry::SortLaneKeys(OutLaneKeys);
	return !OutLaneKeys.IsEmpty();
}
