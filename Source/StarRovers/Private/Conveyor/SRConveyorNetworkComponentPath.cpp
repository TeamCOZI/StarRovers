#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Surface/SRPlanetSurfaceGrid.h"

bool USRConveyorNetworkComponent::FindConveyorPath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& StartCellId,
	const FSRPlanetSurfaceGridCellId& EndCellId,
	int32 Layer,
	TArray<FSRPlanetSurfaceGridCellId>& OutPath) const
{
	OutPath.Reset();
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCell StartCell;
	FSRPlanetSurfaceGridCell EndCell;
	if (!SurfaceGrid->GetCellById(StartCellId, StartCell) || !SurfaceGrid->GetCellById(EndCellId, EndCell))
	{
		return false;
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	TArray<FSRPlanetSurfaceGridCellId> OpenSet;
	TSet<FSRPlanetSurfaceGridCellId> Visited;
	TMap<FSRPlanetSurfaceGridCellId, FSRPlanetSurfaceGridCellId> CameFrom;
	OpenSet.Add(StartCellId);
	Visited.Add(StartCellId);

	for (int32 OpenIndex = 0; OpenIndex < OpenSet.Num(); ++OpenIndex)
	{
		const FSRPlanetSurfaceGridCellId CurrentCellId = OpenSet[OpenIndex];
		if (CurrentCellId == EndCellId)
		{
			TArray<FSRPlanetSurfaceGridCellId> ReversedPath;
			FSRPlanetSurfaceGridCellId TraceCellId = EndCellId;
			ReversedPath.Add(TraceCellId);
			while (!(TraceCellId == StartCellId))
			{
				const FSRPlanetSurfaceGridCellId* PreviousCellId = CameFrom.Find(TraceCellId);
				if (!PreviousCellId)
				{
					return false;
				}
				TraceCellId = *PreviousCellId;
				ReversedPath.Add(TraceCellId);
			}

			OutPath.Reserve(ReversedPath.Num());
			for (int32 PathIndex = ReversedPath.Num() - 1; PathIndex >= 0; --PathIndex)
			{
				OutPath.Add(ReversedPath[PathIndex]);
			}
			return OutPath.Num() > 0;
		}

		FSRPlanetSurfaceGridCellNeighbors Neighbors;
		if (!SurfaceGrid->GetCellNeighbors(CurrentCellId, Neighbors))
		{
			continue;
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
			if (!GetNeighborCellIdByDirection(Neighbors, Direction, NeighborCellId) || Visited.Contains(NeighborCellId))
			{
				continue;
			}

			const FSRConveyorLaneKey NeighborLaneKey = MakeLaneKey(NeighborCellId, SafeLayer);
			if (!(NeighborCellId == EndCellId) && !CanPlaceConveyorSegment(SurfaceGrid, NeighborLaneKey))
			{
				continue;
			}

			Visited.Add(NeighborCellId);
			CameFrom.Add(NeighborCellId, CurrentCellId);
			OpenSet.Add(NeighborCellId);
		}
	}

	return false;
}

FSRConveyorLaneKey USRConveyorNetworkComponent::MakeLaneKey(const FSRPlanetSurfaceGridCellId& CellId, int32 Layer)
{
	FSRConveyorLaneKey LaneKey;
	LaneKey.CellId = CellId;
	LaneKey.Layer = FMath::Max(0, Layer);
	return LaneKey;
}

ESRConveyorGridDirection USRConveyorNetworkComponent::GetOppositeDirection(ESRConveyorGridDirection Direction)
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

ESRConveyorSegmentShape USRConveyorNetworkComponent::ResolveSegmentShape(ESRConveyorGridDirection InputDirection, ESRConveyorGridDirection OutputDirection)
{
	if (InputDirection == ESRConveyorGridDirection::None || OutputDirection == ESRConveyorGridDirection::None)
	{
		return ESRConveyorSegmentShape::End;
	}

	return GetOppositeDirection(InputDirection) == OutputDirection
		? ESRConveyorSegmentShape::Straight
		: ESRConveyorSegmentShape::Corner;
}

bool USRConveyorNetworkComponent::GetNeighborCellIdByDirection(const FSRPlanetSurfaceGridCellNeighbors& Neighbors, ESRConveyorGridDirection Direction, FSRPlanetSurfaceGridCellId& OutCellId)
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

bool USRConveyorNetworkComponent::FindDirectionBetweenCells(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& FromCellId, const FSRPlanetSurfaceGridCellId& ToCellId, ESRConveyorGridDirection& OutDirection)
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

bool USRConveyorNetworkComponent::CanPlaceConveyorSegment(USRPlanetSurfaceGrid* SurfaceGrid, const FSRConveyorLaneKey& LaneKey) const
{
	if (!IsValid(SurfaceGrid) || Segments.Contains(LaneKey))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellInfo CellInfo;
	if (!SurfaceGrid->GetCellInfoById(LaneKey.CellId, CellInfo))
	{
		return false;
	}

	return LaneKey.Layer > 0 || (CellInfo.bCanConstruct && !CellInfo.bOccupied);
}

float USRConveyorNetworkComponent::ResolveConveyorLayerHeight(USRPlanetSurfaceGrid* SurfaceGrid, float RequestedLayerHeight) const
{
	const float TerrainHeightStep = IsValid(SurfaceGrid) ? SurfaceGrid->GetTerrainHeightStep() : 0.0f;
	if (TerrainHeightStep > KINDA_SMALL_NUMBER)
	{
		return TerrainHeightStep;
	}

	if (RequestedLayerHeight > KINDA_SMALL_NUMBER)
	{
		return RequestedLayerHeight;
	}

	return DefaultLayerHeight;
}
