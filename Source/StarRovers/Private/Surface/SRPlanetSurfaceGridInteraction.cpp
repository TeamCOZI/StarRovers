#include "Surface/SRPlanetSurfaceGrid.h"
#include "Surface/SRPlanetSurfaceGridInteractionHelpers.h"

using StarRovers::Surface::Interaction::FSRSurfaceGridInteractionPatchBuilder;

void USRPlanetSurfaceGrid::BeginInteractionHighlightBatch()
{
	++InteractionHighlightBatchDepth;
}

void USRPlanetSurfaceGrid::EndInteractionHighlightBatch()
{
	InteractionHighlightBatchDepth = FMath::Max(0, InteractionHighlightBatchDepth - 1);
	if (InteractionHighlightBatchDepth == 0 && bHasBatchedInteractionHighlightRefresh)
	{
		bHasBatchedInteractionHighlightRefresh = false;
		RefreshInteractionHighlight();
	}
}

bool USRPlanetSurfaceGrid::SetHoveredCell(const FSRPlanetSurfaceGridCellId& CellId)
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex))
	{
		return false;
	}

	if (bHasHoveredCell && HoveredCellId == CellId)
	{
		return true;
	}

	bHasHoveredCell = true;
	HoveredCellId = CellId;
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
	return true;
}

void USRPlanetSurfaceGrid::ClearHoveredCell()
{
	if (!bHasHoveredCell)
	{
		return;
	}

	bHasHoveredCell = false;
	HoveredCellId = FSRPlanetSurfaceGridCellId();
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
}

bool USRPlanetSurfaceGrid::HasHoveredCell() const
{
	return bHasHoveredCell;
}

bool USRPlanetSurfaceGrid::GetHoveredCell(FSRPlanetSurfaceGridCell& OutCell) const
{
	return bHasHoveredCell && GetCellById(HoveredCellId, OutCell);
}

bool USRPlanetSurfaceGrid::GetHoveredCellInfo(FSRPlanetSurfaceGridCellInfo& OutCellInfo) const
{
	return bHasHoveredCell && GetCellInfoById(HoveredCellId, OutCellInfo);
}

bool USRPlanetSurfaceGrid::GetInteractionGridPatchCellIds(
	const FSRPlanetSurfaceGridCellId& CenterCellId,
	TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const
{
	auto IsValidCell = [this](const FSRPlanetSurfaceGridCellId& CellId)
	{
		FSRPlanetSurfaceGridCell Cell;
		return GetCellById(CellId, Cell);
	};

	return FSRSurfaceGridInteractionPatchBuilder::BuildPatchCellIds(
		CenterCellId,
		FaceResolution,
		IsValidCell,
		OutCellIds);
}

bool USRPlanetSurfaceGrid::SetSelectedCell(const FSRPlanetSurfaceGridCellId& CellId)
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex))
	{
		return false;
	}

	if (bHasSelectedCell && SelectedCellId == CellId)
	{
		return true;
	}

	bHasSelectedCell = true;
	SelectedCellId = CellId;
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
	return true;
}

void USRPlanetSurfaceGrid::ClearSelectedCell()
{
	if (!bHasSelectedCell)
	{
		return;
	}

	bHasSelectedCell = false;
	SelectedCellId = FSRPlanetSurfaceGridCellId();
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
}

void USRPlanetSurfaceGrid::SetFacilityPortPreviewCells(
	const TArray<FSRPlanetSurfaceGridCellId>& InputConnectionCellIds,
	const TArray<FSRPlanetSurfaceGridCellId>& OutputConnectionCellIds)
{
	TArray<FSRPlanetSurfaceGridCellId> NewInputPortPreviewCellIds;
	for (const FSRPlanetSurfaceGridCellId& CellId : InputConnectionCellIds)
	{
		int32 CellIndex = INDEX_NONE;
		if (GetCellIndex(CellId, CellIndex))
		{
			NewInputPortPreviewCellIds.AddUnique(CellId);
		}
	}

	TArray<FSRPlanetSurfaceGridCellId> NewOutputPortPreviewCellIds;
	for (const FSRPlanetSurfaceGridCellId& CellId : OutputConnectionCellIds)
	{
		int32 CellIndex = INDEX_NONE;
		if (GetCellIndex(CellId, CellIndex))
		{
			NewOutputPortPreviewCellIds.AddUnique(CellId);
		}
	}

	if (InputPortPreviewCellIds == NewInputPortPreviewCellIds
		&& OutputPortPreviewCellIds == NewOutputPortPreviewCellIds)
	{
		return;
	}

	InputPortPreviewCellIds = MoveTemp(NewInputPortPreviewCellIds);
	OutputPortPreviewCellIds = MoveTemp(NewOutputPortPreviewCellIds);
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
}

void USRPlanetSurfaceGrid::ClearFacilityPortPreviewCells()
{
	if (InputPortPreviewCellIds.IsEmpty() && OutputPortPreviewCellIds.IsEmpty())
	{
		return;
	}

	InputPortPreviewCellIds.Reset();
	OutputPortPreviewCellIds.Reset();
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
}

bool USRPlanetSurfaceGrid::HasSelectedCell() const
{
	return bHasSelectedCell;
}

bool USRPlanetSurfaceGrid::GetSelectedCell(FSRPlanetSurfaceGridCell& OutCell) const
{
	return bHasSelectedCell && GetCellById(SelectedCellId, OutCell);
}

bool USRPlanetSurfaceGrid::GetSelectedCellInfo(FSRPlanetSurfaceGridCellInfo& OutCellInfo) const
{
	return bHasSelectedCell && GetCellInfoById(SelectedCellId, OutCellInfo);
}
