#include "Surface/SRPlanetSurfaceGrid.h"
#include "Surface/SRPlanetSurfaceGridInteractionHelpers.h"

using StarRovers::Surface::Interaction::FSRSurfaceGridInteractionPatchBuilder;

void USRPlanetSurfaceGrid::BeginInteractionHighlightBatch()
{
	InteractionBatch.Begin();
}

void USRPlanetSurfaceGrid::EndInteractionHighlightBatch()
{
	if (InteractionBatch.EndAndShouldRefresh())
	{
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

void USRPlanetSurfaceGrid::SetHoveredInteractionGridPatchVisible(bool bNewVisible)
{
	if (bHoveredInteractionGridPatchVisible == bNewVisible)
	{
		return;
	}

	bHoveredInteractionGridPatchVisible = bNewVisible;
	if (bHasHoveredCell)
	{
		RequestInteractionHighlightRefresh();
		UpdateDebugTickState();
	}
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

void USRPlanetSurfaceGrid::SetAreaSelectionCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	TArray<FSRPlanetSurfaceGridCellId> NewAreaSelectionCellIds;
	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		int32 CellIndex = INDEX_NONE;
		if (GetCellIndex(CellId, CellIndex))
		{
			NewAreaSelectionCellIds.AddUnique(CellId);
		}
	}

	if (AreaSelectionCellIds == NewAreaSelectionCellIds)
	{
		return;
	}

	AreaSelectionCellIds = MoveTemp(NewAreaSelectionCellIds);
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
}

void USRPlanetSurfaceGrid::ClearAreaSelectionCells()
{
	if (AreaSelectionCellIds.IsEmpty())
	{
		return;
	}

	AreaSelectionCellIds.Reset();
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

void USRPlanetSurfaceGrid::SetOccupiedPreviewCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	TArray<FSRPlanetSurfaceGridCellId> NewOccupiedPreviewCellIds;
	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		int32 CellIndex = INDEX_NONE;
		if (GetCellIndex(CellId, CellIndex))
		{
			NewOccupiedPreviewCellIds.AddUnique(CellId);
		}
	}

	if (OccupiedPreviewCellIds == NewOccupiedPreviewCellIds)
	{
		return;
	}

	OccupiedPreviewCellIds = MoveTemp(NewOccupiedPreviewCellIds);
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
}

void USRPlanetSurfaceGrid::ClearOccupiedPreviewCells()
{
	if (OccupiedPreviewCellIds.IsEmpty())
	{
		return;
	}

	OccupiedPreviewCellIds.Reset();
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
}

void USRPlanetSurfaceGrid::SetDeletionPreviewCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	TArray<FSRPlanetSurfaceGridCellId> NewDeletionPreviewCellIds;
	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		int32 CellIndex = INDEX_NONE;
		if (GetCellIndex(CellId, CellIndex))
		{
			NewDeletionPreviewCellIds.AddUnique(CellId);
		}
	}

	if (DeletionPreviewCellIds == NewDeletionPreviewCellIds)
	{
		return;
	}

	DeletionPreviewCellIds = MoveTemp(NewDeletionPreviewCellIds);
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
}

void USRPlanetSurfaceGrid::ClearDeletionPreviewCells()
{
	if (DeletionPreviewCellIds.IsEmpty())
	{
		return;
	}

	DeletionPreviewCellIds.Reset();
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
}

void USRPlanetSurfaceGrid::SetInvalidPreviewCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	TArray<FSRPlanetSurfaceGridCellId> NewInvalidPreviewCellIds;
	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		int32 CellIndex = INDEX_NONE;
		if (GetCellIndex(CellId, CellIndex))
		{
			NewInvalidPreviewCellIds.AddUnique(CellId);
		}
	}

	if (InvalidPreviewCellIds == NewInvalidPreviewCellIds)
	{
		return;
	}

	InvalidPreviewCellIds = MoveTemp(NewInvalidPreviewCellIds);
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
}

void USRPlanetSurfaceGrid::ClearInvalidPreviewCells()
{
	if (InvalidPreviewCellIds.IsEmpty())
	{
		return;
	}

	InvalidPreviewCellIds.Reset();
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
