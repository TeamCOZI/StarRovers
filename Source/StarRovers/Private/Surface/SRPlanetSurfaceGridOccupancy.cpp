#include "Surface/SRPlanetSurfaceGrid.h"

#include "SRPlanetSurfaceGridOccupancyState.h"

namespace SurfaceGridOccupancyState = StarRovers::SurfaceGridOccupancyState;

void USRPlanetSurfaceGrid::ClearOccupancy()
{
	SurfaceGridOccupancyState::ClearOccupancy(
		Cells,
		[this](const FSRPlanetSurfaceGridCell& Cell)
		{
			return BuildCellInfo(Cell);
		},
		[this](const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo)
		{
			return GetStoredCellInfoById(CellId, OutCellInfo);
		},
		[this](const FSRPlanetSurfaceGridCellInfo& CellInfo)
		{
			StoreCellInfo(CellInfo);
		});
	MarkGridMeshDirtyAndRefreshIfVisible();
}

bool USRPlanetSurfaceGrid::SetCellOccupied(const FSRPlanetSurfaceGridCellId& CellId, bool bOccupied, FName OccupantId)
{
	if (!SurfaceGridOccupancyState::SetCellOccupied(
		Cells,
		CellId,
		bOccupied,
		OccupantId,
		[this](const FSRPlanetSurfaceGridCellId& CandidateCellId, int32& OutIndex)
		{
			return GetCellIndex(CandidateCellId, OutIndex);
		},
		[this](const FSRPlanetSurfaceGridCell& Cell)
		{
			return BuildCellInfo(Cell);
		},
		[this](const FSRPlanetSurfaceGridCellId& CandidateCellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo)
		{
			return GetStoredCellInfoById(CandidateCellId, OutCellInfo);
		},
		[this](const FSRPlanetSurfaceGridCellInfo& CellInfo)
		{
			StoreCellInfo(CellInfo);
		}))
	{
		return false;
	}

	MarkGridMeshDirtyAndRefreshIfVisible();
	return true;
}

bool USRPlanetSurfaceGrid::CanOccupyCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds) const
{
	return SurfaceGridOccupancyState::CanOccupyCells(
		CellIds,
		[this](const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo)
		{
			return GetCellInfoById(CellId, OutCellInfo);
		});
}

bool USRPlanetSurfaceGrid::SetCellsOccupied(const TArray<FSRPlanetSurfaceGridCellId>& CellIds, bool bOccupied, FName OccupantId)
{
	if (!SurfaceGridOccupancyState::SetCellsOccupied(
		Cells,
		CellIds,
		bOccupied,
		OccupantId,
		[this](const FSRPlanetSurfaceGridCellId& CellId, int32& OutIndex)
		{
			return GetCellIndex(CellId, OutIndex);
		},
		[this](const FSRPlanetSurfaceGridCell& Cell)
		{
			return BuildCellInfo(Cell);
		},
		[this](const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo)
		{
			return GetStoredCellInfoById(CellId, OutCellInfo);
		},
		[this](const FSRPlanetSurfaceGridCellInfo& CellInfo)
		{
			StoreCellInfo(CellInfo);
		}))
	{
		return false;
	}

	MarkGridMeshDirtyAndRefreshIfVisible();
	return true;
}
