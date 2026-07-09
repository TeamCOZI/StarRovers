#include "Conveyor/SRConveyorSegmentBuilder.h"

#include "Conveyor/SRConveyorNetworkGeometry.h"
#include "Conveyor/SRConveyorSegmentMerger.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool StarRovers::Conveyor::FSRConveyorSegmentBuilder::BuildPathSegments(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
	int32 Layer,
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& ExistingSegments,
	TArray<FSRConveyorSegment>& OutSegments,
	FName NetworkId,
	USRStructureDataAsset* StructureDataAsset)
{
	OutSegments.Reset();
	if (!IsValid(SurfaceGrid) || PathCellIds.IsEmpty())
	{
		return false;
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	OutSegments.Reserve(PathCellIds.Num());
	for (int32 PathIndex = 0; PathIndex < PathCellIds.Num(); ++PathIndex)
	{
		const FSRPlanetSurfaceGridCellId& CellId = PathCellIds[PathIndex];
		ESRConveyorGridDirection InputDirection = ESRConveyorGridDirection::None;
		ESRConveyorGridDirection OutputDirection = ESRConveyorGridDirection::None;
		if (PathIndex > 0)
		{
			ESRConveyorGridDirection PreviousDirection = ESRConveyorGridDirection::None;
			if (FSRConveyorNetworkGeometry::FindDirectionBetweenCells(SurfaceGrid, CellId, PathCellIds[PathIndex - 1], PreviousDirection))
			{
				InputDirection = PreviousDirection;
			}
		}
		if (PathIndex + 1 < PathCellIds.Num())
		{
			FSRConveyorNetworkGeometry::FindDirectionBetweenCells(SurfaceGrid, CellId, PathCellIds[PathIndex + 1], OutputDirection);
		}

		FSRConveyorSegment Segment;
		Segment.Lane = FSRConveyorNetworkGeometry::MakeLaneKey(CellId, SafeLayer);
		Segment.InputDirection = InputDirection;
		Segment.OutputDirection = OutputDirection;
		Segment.Shape = FSRConveyorNetworkGeometry::ResolveSegmentShape(InputDirection, OutputDirection);
		Segment.NetworkId = NetworkId;
		Segment.StructureDataAsset = StructureDataAsset;
		if (!FSRConveyorSegmentMerger::CanMergeSegment(ExistingSegments, Segment))
		{
			OutSegments.Reset();
			return false;
		}

		OutSegments.Add(Segment);
	}

	return true;
}

void StarRovers::Conveyor::FSRConveyorSegmentBuilder::RebuildFromBeltPaths(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRConveyorBeltPath>& BeltPaths,
	TMap<FSRConveyorLaneKey, FSRConveyorSegment>& OutSegments)
{
	OutSegments.Reset();

	for (const FSRConveyorBeltPath& BeltPath : BeltPaths)
	{
		for (int32 PathIndex = 0; PathIndex < BeltPath.CellIds.Num(); ++PathIndex)
		{
			const FSRPlanetSurfaceGridCellId& CellId = BeltPath.CellIds[PathIndex];
			ESRConveyorGridDirection InputDirection = ESRConveyorGridDirection::None;
			ESRConveyorGridDirection OutputDirection = ESRConveyorGridDirection::None;
			if (PathIndex > 0)
			{
				FSRConveyorNetworkGeometry::FindDirectionBetweenCells(SurfaceGrid, CellId, BeltPath.CellIds[PathIndex - 1], InputDirection);
			}
			if (PathIndex + 1 < BeltPath.CellIds.Num())
			{
				FSRConveyorNetworkGeometry::FindDirectionBetweenCells(SurfaceGrid, CellId, BeltPath.CellIds[PathIndex + 1], OutputDirection);
			}

			FSRConveyorSegment Segment;
			Segment.Lane = FSRConveyorNetworkGeometry::MakeLaneKey(CellId, BeltPath.Layer);
			Segment.InputDirection = InputDirection;
			Segment.OutputDirection = OutputDirection;
			Segment.Shape = FSRConveyorNetworkGeometry::ResolveSegmentShape(InputDirection, OutputDirection);
			Segment.NetworkId = BeltPath.NetworkId;
			Segment.StructureDataAsset = BeltPath.StructureDataAsset;
			FSRConveyorSegmentMerger::MergeSegment(OutSegments, Segment);
		}
	}
}
