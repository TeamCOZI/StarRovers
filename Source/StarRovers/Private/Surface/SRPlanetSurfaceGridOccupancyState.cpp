#include "SRPlanetSurfaceGridOccupancyState.h"

namespace
{
	void StoreUpdatedOccupancyCellInfo(
		const FSRPlanetSurfaceGridCell& Cell,
		StarRovers::SurfaceGridOccupancyState::FCellInfoBuilder BuildCellInfo,
		StarRovers::SurfaceGridOccupancyState::FCellInfoQuery GetStoredCellInfoById,
		StarRovers::SurfaceGridOccupancyState::FCellInfoStore StoreCellInfo)
	{
		FSRPlanetSurfaceGridCellInfo UpdatedCellInfo = BuildCellInfo(Cell);
		FSRPlanetSurfaceGridCellInfo ExistingCellInfo;
		if (GetStoredCellInfoById(Cell.CellId, ExistingCellInfo))
		{
			UpdatedCellInfo.FaceCellIndex = ExistingCellInfo.FaceCellIndex;
		}
		StoreCellInfo(UpdatedCellInfo);
	}

	void ApplyCellOccupancy(
		FSRPlanetSurfaceGridCell& Cell,
		bool bOccupied,
		FName OccupantId,
		StarRovers::SurfaceGridOccupancyState::FCellInfoBuilder BuildCellInfo,
		StarRovers::SurfaceGridOccupancyState::FCellInfoQuery GetStoredCellInfoById,
		StarRovers::SurfaceGridOccupancyState::FCellInfoStore StoreCellInfo)
	{
		Cell.bOccupied = bOccupied;
		Cell.OccupantId = bOccupied ? OccupantId : NAME_None;
		StoreUpdatedOccupancyCellInfo(Cell, BuildCellInfo, GetStoredCellInfoById, StoreCellInfo);
	}
}

void StarRovers::SurfaceGridOccupancyState::ClearOccupancy(
	TArray<FSRPlanetSurfaceGridCell>& Cells,
	FCellInfoBuilder BuildCellInfo,
	FCellInfoQuery GetStoredCellInfoById,
	FCellInfoStore StoreCellInfo)
{
	for (FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		ApplyCellOccupancy(Cell, false, NAME_None, BuildCellInfo, GetStoredCellInfoById, StoreCellInfo);
	}
}

bool StarRovers::SurfaceGridOccupancyState::SetCellOccupied(
	TArray<FSRPlanetSurfaceGridCell>& Cells,
	const FSRPlanetSurfaceGridCellId& CellId,
	bool bOccupied,
	FName OccupantId,
	FCellIndexQuery GetCellIndex,
	FCellInfoBuilder BuildCellInfo,
	FCellInfoQuery GetStoredCellInfoById,
	FCellInfoStore StoreCellInfo)
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex) || !Cells.IsValidIndex(CellIndex))
	{
		return false;
	}

	ApplyCellOccupancy(Cells[CellIndex], bOccupied, OccupantId, BuildCellInfo, GetStoredCellInfoById, StoreCellInfo);
	return true;
}

bool StarRovers::SurfaceGridOccupancyState::CanOccupyCells(
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	FCellInfoQuery GetCellInfoById)
{
	if (CellIds.IsEmpty())
	{
		return false;
	}

	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!GetCellInfoById(CellId, CellInfo) || !CellInfo.bCanConstruct || CellInfo.bOccupied)
		{
			return false;
		}
	}

	return true;
}

bool StarRovers::SurfaceGridOccupancyState::SetCellsOccupied(
	TArray<FSRPlanetSurfaceGridCell>& Cells,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	bool bOccupied,
	FName OccupantId,
	FCellIndexQuery GetCellIndex,
	FCellInfoBuilder BuildCellInfo,
	FCellInfoQuery GetStoredCellInfoById,
	FCellInfoStore StoreCellInfo)
{
	if (CellIds.IsEmpty())
	{
		return false;
	}

	TArray<int32> CellIndices;
	CellIndices.Reserve(CellIds.Num());
	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		int32 CellIndex = INDEX_NONE;
		if (!GetCellIndex(CellId, CellIndex) || !Cells.IsValidIndex(CellIndex))
		{
			return false;
		}

		CellIndices.Add(CellIndex);
	}

	for (int32 CellIndex : CellIndices)
	{
		ApplyCellOccupancy(Cells[CellIndex], bOccupied, OccupantId, BuildCellInfo, GetStoredCellInfoById, StoreCellInfo);
	}

	return true;
}
