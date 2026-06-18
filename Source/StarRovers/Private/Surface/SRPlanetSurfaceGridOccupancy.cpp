#include "Surface/SRPlanetSurfaceGrid.h"

void USRPlanetSurfaceGrid::ClearOccupancy()
{
	for (FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		Cell.bOccupied = false;
		Cell.OccupantId = NAME_None;
		FSRPlanetSurfaceGridCellInfo UpdatedCellInfo = BuildCellInfo(Cell);
		FSRPlanetSurfaceGridCellInfo ExistingCellInfo;
		if (GetStoredCellInfoById(Cell.CellId, ExistingCellInfo))
		{
			UpdatedCellInfo.FaceCellIndex = ExistingCellInfo.FaceCellIndex;
		}
		StoreCellInfo(UpdatedCellInfo);
	}
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RequestInteractionHighlightRefresh();
	}
}

bool USRPlanetSurfaceGrid::SetCellOccupied(const FSRPlanetSurfaceGridCellId& CellId, bool bOccupied, FName OccupantId)
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex))
	{
		return false;
	}

	FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
	Cell.bOccupied = bOccupied;
	Cell.OccupantId = bOccupied ? OccupantId : NAME_None;
	FSRPlanetSurfaceGridCellInfo UpdatedCellInfo = BuildCellInfo(Cell);
	FSRPlanetSurfaceGridCellInfo ExistingCellInfo;
	if (GetStoredCellInfoById(CellId, ExistingCellInfo))
	{
		UpdatedCellInfo.FaceCellIndex = ExistingCellInfo.FaceCellIndex;
	}
	StoreCellInfo(UpdatedCellInfo);
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RequestInteractionHighlightRefresh();
	}
	return true;
}

bool USRPlanetSurfaceGrid::CanOccupyCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds) const
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

bool USRPlanetSurfaceGrid::SetCellsOccupied(const TArray<FSRPlanetSurfaceGridCellId>& CellIds, bool bOccupied, FName OccupantId)
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
		if (!GetCellIndex(CellId, CellIndex))
		{
			return false;
		}

		CellIndices.Add(CellIndex);
	}

	for (int32 CellIndex : CellIndices)
	{
		FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
		Cell.bOccupied = bOccupied;
		Cell.OccupantId = bOccupied ? OccupantId : NAME_None;
		FSRPlanetSurfaceGridCellInfo UpdatedCellInfo = BuildCellInfo(Cell);
		FSRPlanetSurfaceGridCellInfo ExistingCellInfo;
		if (GetStoredCellInfoById(Cell.CellId, ExistingCellInfo))
		{
			UpdatedCellInfo.FaceCellIndex = ExistingCellInfo.FaceCellIndex;
		}
		StoreCellInfo(UpdatedCellInfo);
	}

	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RequestInteractionHighlightRefresh();
	}
	return true;
}