#include "Conveyor/SRConveyorConnectionQuery.h"

#include "Conveyor/SRConveyorNetworkGeometry.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	bool TryResolveLaneByDirectionFromNeighbors(
		const FSRPlanetSurfaceGridCellNeighbors& Neighbors,
		int32 Layer,
		ESRConveyorGridDirection Direction,
		FSRConveyorLaneKey& OutLaneKey)
	{
		OutLaneKey = FSRConveyorLaneKey();
		if (Direction == ESRConveyorGridDirection::None)
		{
			return false;
		}

		FSRPlanetSurfaceGridCellId NextCellId;
		if (!StarRovers::Conveyor::FSRConveyorNetworkGeometry::GetNeighborCellIdByDirection(Neighbors, Direction, NextCellId))
		{
			return false;
		}

		OutLaneKey = StarRovers::Conveyor::FSRConveyorNetworkGeometry::MakeLaneKey(NextCellId, Layer);
		return true;
	}

	bool DoesDirectionReferenceLane(
		const FSRPlanetSurfaceGridCellNeighbors& Neighbors,
		int32 Layer,
		ESRConveyorGridDirection Direction,
		const FSRConveyorLaneKey& TargetLaneKey)
	{
		FSRConveyorLaneKey NeighborLaneKey;
		return TryResolveLaneByDirectionFromNeighbors(Neighbors, Layer, Direction, NeighborLaneKey)
			&& NeighborLaneKey == TargetLaneKey;
	}

	bool DoesSegmentReferenceLaneWithNeighbors(
		const FSRConveyorSegment& Segment,
		const FSRPlanetSurfaceGridCellNeighbors& Neighbors,
		const FSRConveyorLaneKey& TargetLaneKey)
	{
		if (Segment.Lane.Layer != TargetLaneKey.Layer)
		{
			return false;
		}

		return DoesDirectionReferenceLane(Neighbors, Segment.Lane.Layer, Segment.InputDirection, TargetLaneKey)
			|| DoesDirectionReferenceLane(Neighbors, Segment.Lane.Layer, Segment.MergeInputDirection, TargetLaneKey)
			|| DoesDirectionReferenceLane(Neighbors, Segment.Lane.Layer, Segment.SecondMergeInputDirection, TargetLaneKey)
			|| DoesDirectionReferenceLane(Neighbors, Segment.Lane.Layer, Segment.OutputDirection, TargetLaneKey)
			|| DoesDirectionReferenceLane(Neighbors, Segment.Lane.Layer, Segment.BranchOutputDirection, TargetLaneKey)
			|| DoesDirectionReferenceLane(Neighbors, Segment.Lane.Layer, Segment.SecondBranchOutputDirection, TargetLaneKey);
	}
}

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

	return TryResolveLaneByDirectionFromNeighbors(Neighbors, Segment.Lane.Layer, Direction, OutLaneKey);
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

	FSRPlanetSurfaceGridCellNeighbors Neighbors;
	return SurfaceGrid->GetCellNeighbors(Segment.Lane.CellId, Neighbors)
		&& DoesSegmentReferenceLaneWithNeighbors(Segment, Neighbors, TargetLaneKey);
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
	const FSRConveyorSegment* StartSegment = Segments.Find(StartLaneKey);
	if (!StartSegment)
	{
		return false;
	}

	TSet<FSRConveyorLaneKey> VisitedLaneKeys;
	TArray<FSRConveyorLaneKey> OpenLaneKeys;
	const int32 InitialSearchCapacity = FMath::Min(Segments.Num(), 256);
	VisitedLaneKeys.Reserve(InitialSearchCapacity);
	OpenLaneKeys.Reserve(InitialSearchCapacity);
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

		FSRPlanetSurfaceGridCellNeighbors CurrentNeighbors;
		if (!SurfaceGrid->GetCellNeighbors(CurrentSegment->Lane.CellId, CurrentNeighbors))
		{
			continue;
		}

		for (const ESRConveyorGridDirection Direction : Directions)
		{
			FSRConveyorLaneKey NeighborLaneKey;
			if (!TryResolveLaneByDirectionFromNeighbors(CurrentNeighbors, CurrentSegment->Lane.Layer, Direction, NeighborLaneKey)
				|| VisitedLaneKeys.Contains(NeighborLaneKey))
			{
				continue;
			}

			const FSRConveyorSegment* NeighborSegment = Segments.Find(NeighborLaneKey);
			if (!NeighborSegment)
			{
				continue;
			}

			const bool bCurrentReferencesNeighbor = DoesSegmentReferenceLaneWithNeighbors(*CurrentSegment, CurrentNeighbors, NeighborLaneKey);
			bool bNeighborReferencesCurrent = false;
			if (!bCurrentReferencesNeighbor)
			{
				FSRPlanetSurfaceGridCellNeighbors NeighborNeighbors;
				bNeighborReferencesCurrent = SurfaceGrid->GetCellNeighbors(NeighborSegment->Lane.CellId, NeighborNeighbors)
					&& DoesSegmentReferenceLaneWithNeighbors(*NeighborSegment, NeighborNeighbors, CurrentLaneKey);
			}

			if (!bCurrentReferencesNeighbor && !bNeighborReferencesCurrent)
			{
				continue;
			}

			VisitedLaneKeys.Add(NeighborLaneKey);
			OpenLaneKeys.Add(NeighborLaneKey);
		}
	}

	OutLaneKeys = MoveTemp(OpenLaneKeys);
	FSRConveyorNetworkGeometry::SortLaneKeys(OutLaneKeys);
	return !OutLaneKeys.IsEmpty();
}
