#include "SRPlanetSurfaceGridInteractionState.h"

namespace
{
	TArray<FSRPlanetSurfaceGridCellId> BuildValidatedUniqueCellIds(
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
		StarRovers::SurfaceGridInteractionState::FCellIdValidator IsValidCell)
	{
		TArray<FSRPlanetSurfaceGridCellId> ValidCellIds;
		ValidCellIds.Reserve(CellIds.Num());
		TSet<FSRPlanetSurfaceGridCellId> SeenCellIds;
		SeenCellIds.Reserve(CellIds.Num());
		for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
		{
			if (!IsValidCell(CellId))
			{
				continue;
			}

			bool bAlreadySeen = false;
			SeenCellIds.Add(CellId, &bAlreadySeen);
			if (!bAlreadySeen)
			{
				ValidCellIds.Add(CellId);
			}
		}

		return ValidCellIds;
	}
}

bool StarRovers::SurfaceGridInteractionState::SetFocusedCell(
	const FSRPlanetSurfaceGridCellId& CellId,
	bool& bHasCell,
	FSRPlanetSurfaceGridCellId& StoredCellId,
	FCellIdValidator IsValidCell,
	bool& bOutChanged)
{
	bOutChanged = false;
	if (!IsValidCell(CellId))
	{
		return false;
	}

	if (bHasCell && StoredCellId == CellId)
	{
		return true;
	}

	bHasCell = true;
	StoredCellId = CellId;
	bOutChanged = true;
	return true;
}

bool StarRovers::SurfaceGridInteractionState::ClearFocusedCell(
	bool& bHasCell,
	FSRPlanetSurfaceGridCellId& StoredCellId)
{
	if (!bHasCell)
	{
		return false;
	}

	bHasCell = false;
	StoredCellId = FSRPlanetSurfaceGridCellId();
	return true;
}

bool StarRovers::SurfaceGridInteractionState::SetValidatedUniqueCellIds(
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	TArray<FSRPlanetSurfaceGridCellId>& StoredCellIds,
	FCellIdValidator IsValidCell)
{
	TArray<FSRPlanetSurfaceGridCellId> NewCellIds = BuildValidatedUniqueCellIds(CellIds, IsValidCell);
	if (StoredCellIds == NewCellIds)
	{
		return false;
	}

	StoredCellIds = MoveTemp(NewCellIds);
	return true;
}

bool StarRovers::SurfaceGridInteractionState::ClearCellIds(TArray<FSRPlanetSurfaceGridCellId>& StoredCellIds)
{
	if (StoredCellIds.IsEmpty())
	{
		return false;
	}

	StoredCellIds.Reset();
	return true;
}

bool StarRovers::SurfaceGridInteractionState::SetValidatedPortPreviewCellIds(
	const TArray<FSRPlanetSurfaceGridCellId>& InputConnectionCellIds,
	const TArray<FSRPlanetSurfaceGridCellId>& OutputConnectionCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& StoredInputCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& StoredOutputCellIds,
	FCellIdValidator IsValidCell)
{
	TArray<FSRPlanetSurfaceGridCellId> NewInputCellIds = BuildValidatedUniqueCellIds(InputConnectionCellIds, IsValidCell);
	TArray<FSRPlanetSurfaceGridCellId> NewOutputCellIds = BuildValidatedUniqueCellIds(OutputConnectionCellIds, IsValidCell);
	if (StoredInputCellIds == NewInputCellIds && StoredOutputCellIds == NewOutputCellIds)
	{
		return false;
	}

	StoredInputCellIds = MoveTemp(NewInputCellIds);
	StoredOutputCellIds = MoveTemp(NewOutputCellIds);
	return true;
}

bool StarRovers::SurfaceGridInteractionState::ClearPortPreviewCellIds(
	TArray<FSRPlanetSurfaceGridCellId>& StoredInputCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& StoredOutputCellIds)
{
	if (StoredInputCellIds.IsEmpty() && StoredOutputCellIds.IsEmpty())
	{
		return false;
	}

	StoredInputCellIds.Reset();
	StoredOutputCellIds.Reset();
	return true;
}

bool StarRovers::SurfaceGridInteractionState::HasInteractionOverlayContent(
	bool bHasHoveredCell,
	bool bHasSelectedCell,
	const TArray<FSRPlanetSurfaceGridCellId>& AreaSelectionCellIds,
	const TArray<FSRPlanetSurfaceGridCellId>& OccupiedPreviewCellIds,
	const TArray<FSRPlanetSurfaceGridCellId>& InputPortPreviewCellIds,
	const TArray<FSRPlanetSurfaceGridCellId>& OutputPortPreviewCellIds,
	const TArray<FSRPlanetSurfaceGridCellId>& DeletionPreviewCellIds,
	const TArray<FSRPlanetSurfaceGridCellId>& ConstructionReplacementPreviewCellIds,
	const TArray<FSRPlanetSurfaceGridCellId>& InvalidPreviewCellIds)
{
	return bHasHoveredCell
		|| bHasSelectedCell
		|| !AreaSelectionCellIds.IsEmpty()
		|| !OccupiedPreviewCellIds.IsEmpty()
		|| !InputPortPreviewCellIds.IsEmpty()
		|| !OutputPortPreviewCellIds.IsEmpty()
		|| !DeletionPreviewCellIds.IsEmpty()
		|| !ConstructionReplacementPreviewCellIds.IsEmpty()
		|| !InvalidPreviewCellIds.IsEmpty();
}

void StarRovers::SurfaceGridInteractionState::ResetInteractionState(
	bool& bHasHoveredCell,
	FSRPlanetSurfaceGridCellId& HoveredCellId,
	bool& bHoveredInteractionGridPatchVisible,
	bool& bHasSelectedCell,
	FSRPlanetSurfaceGridCellId& SelectedCellId,
	TArray<FSRPlanetSurfaceGridCellId>& AreaSelectionCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& OccupiedPreviewCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& InputPortPreviewCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& OutputPortPreviewCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& DeletionPreviewCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& ConstructionReplacementPreviewCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& InvalidPreviewCellIds)
{
	bHasHoveredCell = false;
	HoveredCellId = FSRPlanetSurfaceGridCellId();
	bHoveredInteractionGridPatchVisible = true;
	bHasSelectedCell = false;
	SelectedCellId = FSRPlanetSurfaceGridCellId();
	AreaSelectionCellIds.Reset();
	OccupiedPreviewCellIds.Reset();
	InputPortPreviewCellIds.Reset();
	OutputPortPreviewCellIds.Reset();
	DeletionPreviewCellIds.Reset();
	ConstructionReplacementPreviewCellIds.Reset();
	InvalidPreviewCellIds.Reset();
}
