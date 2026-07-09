#include "Conveyor/SRConveyorSegmentQuery.h"

bool StarRovers::Conveyor::FSRConveyorSegmentQuery::HasSegmentAtCell(
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const FSRPlanetSurfaceGridCellId& CellId)
{
	for (const TPair<FSRConveyorLaneKey, FSRConveyorSegment>& SegmentPair : Segments)
	{
		if (SegmentPair.Key.CellId == CellId)
		{
			return true;
		}
	}

	return false;
}

void StarRovers::Conveyor::FSRConveyorSegmentQuery::GatherCellIdsAtLayer(
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	int32 Layer,
	TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
{
	OutCellIds.Reset();
	const int32 SafeLayer = FMath::Max(0, Layer);
	for (const TPair<FSRConveyorLaneKey, FSRConveyorSegment>& SegmentPair : Segments)
	{
		if (SegmentPair.Key.Layer == SafeLayer)
		{
			OutCellIds.Add(SegmentPair.Key.CellId);
		}
	}
}
