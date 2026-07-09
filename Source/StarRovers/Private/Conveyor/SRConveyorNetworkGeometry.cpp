#include "Conveyor/SRConveyorNetworkGeometry.h"

#include "Surface/SRPlanetSurfaceGrid.h"

FSRConveyorLaneKey StarRovers::Conveyor::FSRConveyorNetworkGeometry::MakeLaneKey(
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer)
{
	FSRConveyorLaneKey LaneKey;
	LaneKey.CellId = CellId;
	LaneKey.Layer = FMath::Max(0, Layer);
	return LaneKey;
}

void StarRovers::Conveyor::FSRConveyorNetworkGeometry::SortLaneKeys(TArray<FSRConveyorLaneKey>& LaneKeys)
{
	LaneKeys.Sort([](const FSRConveyorLaneKey& Left, const FSRConveyorLaneKey& Right)
	{
		const int32 LeftFace = static_cast<int32>(Left.CellId.Face);
		const int32 RightFace = static_cast<int32>(Right.CellId.Face);
		if (LeftFace != RightFace)
		{
			return LeftFace < RightFace;
		}
		if (Left.CellId.CellY != Right.CellId.CellY)
		{
			return Left.CellId.CellY < Right.CellId.CellY;
		}
		if (Left.CellId.CellX != Right.CellId.CellX)
		{
			return Left.CellId.CellX < Right.CellId.CellX;
		}
		return Left.Layer < Right.Layer;
	});
}

ESRConveyorGridDirection StarRovers::Conveyor::FSRConveyorNetworkGeometry::GetOppositeDirection(
	ESRConveyorGridDirection Direction)
{
	switch (Direction)
	{
	case ESRConveyorGridDirection::NegativeU:
		return ESRConveyorGridDirection::PositiveU;
	case ESRConveyorGridDirection::PositiveU:
		return ESRConveyorGridDirection::NegativeU;
	case ESRConveyorGridDirection::NegativeV:
		return ESRConveyorGridDirection::PositiveV;
	case ESRConveyorGridDirection::PositiveV:
		return ESRConveyorGridDirection::NegativeV;
	default:
		return ESRConveyorGridDirection::None;
	}
}

ESRConveyorSegmentShape StarRovers::Conveyor::FSRConveyorNetworkGeometry::ResolveSegmentShape(
	ESRConveyorGridDirection InputDirection,
	ESRConveyorGridDirection OutputDirection)
{
	if (InputDirection == ESRConveyorGridDirection::None || OutputDirection == ESRConveyorGridDirection::None)
	{
		return ESRConveyorSegmentShape::End;
	}

	return GetOppositeDirection(InputDirection) == OutputDirection
		? ESRConveyorSegmentShape::Straight
		: ESRConveyorSegmentShape::Corner;
}

bool StarRovers::Conveyor::FSRConveyorNetworkGeometry::GetNeighborCellIdByDirection(
	const FSRPlanetSurfaceGridCellNeighbors& Neighbors,
	ESRConveyorGridDirection Direction,
	FSRPlanetSurfaceGridCellId& OutCellId)
{
	switch (Direction)
	{
	case ESRConveyorGridDirection::NegativeU:
		OutCellId = Neighbors.NegativeU;
		return true;
	case ESRConveyorGridDirection::PositiveU:
		OutCellId = Neighbors.PositiveU;
		return true;
	case ESRConveyorGridDirection::NegativeV:
		OutCellId = Neighbors.NegativeV;
		return true;
	case ESRConveyorGridDirection::PositiveV:
		OutCellId = Neighbors.PositiveV;
		return true;
	default:
		OutCellId = FSRPlanetSurfaceGridCellId();
		return false;
	}
}

bool StarRovers::Conveyor::FSRConveyorNetworkGeometry::FindDirectionBetweenCells(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& FromCellId,
	const FSRPlanetSurfaceGridCellId& ToCellId,
	ESRConveyorGridDirection& OutDirection)
{
	OutDirection = ESRConveyorGridDirection::None;
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellNeighbors Neighbors;
	if (!SurfaceGrid->GetCellNeighbors(FromCellId, Neighbors))
	{
		return false;
	}

	const ESRConveyorGridDirection Directions[] =
	{
		ESRConveyorGridDirection::NegativeU,
		ESRConveyorGridDirection::PositiveU,
		ESRConveyorGridDirection::NegativeV,
		ESRConveyorGridDirection::PositiveV,
	};
	for (const ESRConveyorGridDirection Direction : Directions)
	{
		FSRPlanetSurfaceGridCellId NeighborCellId;
		if (GetNeighborCellIdByDirection(Neighbors, Direction, NeighborCellId) && NeighborCellId == ToCellId)
		{
			OutDirection = Direction;
			return true;
		}
	}

	return false;
}

int32 StarRovers::Conveyor::FSRConveyorNetworkGeometry::GetDirectionClockwiseOrder(
	ESRConveyorGridDirection Direction)
{
	switch (Direction)
	{
	case ESRConveyorGridDirection::NegativeV:
		return 0;
	case ESRConveyorGridDirection::PositiveU:
		return 1;
	case ESRConveyorGridDirection::PositiveV:
		return 2;
	case ESRConveyorGridDirection::NegativeU:
		return 3;
	default:
		return MAX_int32;
	}
}

void StarRovers::Conveyor::FSRConveyorNetworkGeometry::SortDirectionsClockwise(
	TArray<ESRConveyorGridDirection>& Directions)
{
	Directions.Sort([](ESRConveyorGridDirection Left, ESRConveyorGridDirection Right)
	{
		return FSRConveyorNetworkGeometry::GetDirectionClockwiseOrder(Left)
			< FSRConveyorNetworkGeometry::GetDirectionClockwiseOrder(Right);
	});
}

void StarRovers::Conveyor::FSRConveyorNetworkGeometry::CollectInputDirections(
	const FSRConveyorSegment& Segment,
	TArray<ESRConveyorGridDirection>& OutDirections)
{
	OutDirections.Reset();
	auto AddDirection = [&OutDirections](ESRConveyorGridDirection Direction)
	{
		if (Direction != ESRConveyorGridDirection::None && !OutDirections.Contains(Direction))
		{
			OutDirections.Add(Direction);
		}
	};

	AddDirection(Segment.InputDirection);
	AddDirection(Segment.MergeInputDirection);
	AddDirection(Segment.SecondMergeInputDirection);
	SortDirectionsClockwise(OutDirections);
}

void StarRovers::Conveyor::FSRConveyorNetworkGeometry::CollectOutputDirections(
	const FSRConveyorSegment& Segment,
	TArray<ESRConveyorGridDirection>& OutDirections)
{
	OutDirections.Reset();
	auto AddDirection = [&OutDirections](ESRConveyorGridDirection Direction)
	{
		if (Direction != ESRConveyorGridDirection::None && !OutDirections.Contains(Direction))
		{
			OutDirections.Add(Direction);
		}
	};

	AddDirection(Segment.OutputDirection);
	AddDirection(Segment.BranchOutputDirection);
	AddDirection(Segment.SecondBranchOutputDirection);
	SortDirectionsClockwise(OutDirections);
}
