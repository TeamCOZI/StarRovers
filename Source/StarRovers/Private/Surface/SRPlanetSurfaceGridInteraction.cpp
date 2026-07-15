#include "Surface/SRPlanetSurfaceGrid.h"
#include "Surface/SRPlanetSurfaceGridInteractionCoordinateMapping.h"

using StarRovers::Surface::Interaction::FSRPlanetSurfaceGridInteractionPatchBuilder;

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
	return SetFocusedInteractionCell(CellId, bHasHoveredCell, HoveredCellId);
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
		NotifyInteractionStateChanged();
	}
}

void USRPlanetSurfaceGrid::ClearHoveredCell()
{
	ClearFocusedInteractionCell(bHasHoveredCell, HoveredCellId);
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
		return IsInteractionCellIdValid(CellId);
	};

	return FSRPlanetSurfaceGridInteractionPatchBuilder::BuildPatchCellIds(
		CenterCellId,
		FaceResolution,
		IsValidCell,
		OutCellIds);
}

bool USRPlanetSurfaceGrid::SetSelectedCell(const FSRPlanetSurfaceGridCellId& CellId)
{
	return SetFocusedInteractionCell(CellId, bHasSelectedCell, SelectedCellId);
}

void USRPlanetSurfaceGrid::ClearSelectedCell()
{
	ClearFocusedInteractionCell(bHasSelectedCell, SelectedCellId);
}

void USRPlanetSurfaceGrid::SetAreaSelectionCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	SetInteractionPreviewCellIds(CellIds, AreaSelectionCellIds);
}

void USRPlanetSurfaceGrid::ClearAreaSelectionCells()
{
	ClearInteractionPreviewCellIds(AreaSelectionCellIds);
}

void USRPlanetSurfaceGrid::SetFacilityPortPreviewCells(
	const TArray<FSRPlanetSurfaceGridCellId>& InputConnectionCellIds,
	const TArray<FSRPlanetSurfaceGridCellId>& OutputConnectionCellIds)
{
	SetInteractionPortPreviewCellIds(InputConnectionCellIds, OutputConnectionCellIds);
}

void USRPlanetSurfaceGrid::ClearFacilityPortPreviewCells()
{
	ClearInteractionPortPreviewCellIds();
}

void USRPlanetSurfaceGrid::SetOccupiedPreviewCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	SetInteractionPreviewCellIds(CellIds, OccupiedPreviewCellIds);
}

void USRPlanetSurfaceGrid::ClearOccupiedPreviewCells()
{
	ClearInteractionPreviewCellIds(OccupiedPreviewCellIds);
}

void USRPlanetSurfaceGrid::SetDeletionPreviewCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	SetInteractionPreviewCellIds(CellIds, DeletionPreviewCellIds);
}

void USRPlanetSurfaceGrid::ClearDeletionPreviewCells()
{
	ClearInteractionPreviewCellIds(DeletionPreviewCellIds);
}

void USRPlanetSurfaceGrid::SetConstructionReplacementPreviewCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	SetInteractionPreviewCellIds(CellIds, ConstructionReplacementPreviewCellIds);
}

void USRPlanetSurfaceGrid::ClearConstructionReplacementPreviewCells()
{
	ClearInteractionPreviewCellIds(ConstructionReplacementPreviewCellIds);
}

void USRPlanetSurfaceGrid::SetInvalidPreviewCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	SetInteractionPreviewCellIds(CellIds, InvalidPreviewCellIds);
}

void USRPlanetSurfaceGrid::ClearInvalidPreviewCells()
{
	ClearInteractionPreviewCellIds(InvalidPreviewCellIds);
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
