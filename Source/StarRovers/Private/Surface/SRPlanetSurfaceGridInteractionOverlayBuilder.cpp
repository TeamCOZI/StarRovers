#include "SRPlanetSurfaceGridInteractionOverlayBuilder.h"

#include "DynamicMesh/DynamicMesh3.h"

namespace
{
	bool HasAnyCells(const TArray<FSRPlanetSurfaceGridCellId>* CellIds)
	{
		return CellIds && !CellIds->IsEmpty();
	}

	bool ContainsCell(const TArray<FSRPlanetSurfaceGridCellId>* CellIds, const FSRPlanetSurfaceGridCellId& CellId)
	{
		return CellIds && CellIds->Contains(CellId);
	}

	void AppendPreviewCells(
		UE::Geometry::FDynamicMesh3& OverlayMesh,
		const TArray<FSRPlanetSurfaceGridCellId>* CellIds,
		const FLinearColor& CellColor,
		float LineThickness,
		StarRovers::SurfaceGridInteractionOverlayBuilder::FCellLookup GetCellById,
		StarRovers::SurfaceGridInteractionOverlayBuilder::FAppendInteractionCell AppendInteractionCell)
	{
		if (!HasAnyCells(CellIds))
		{
			return;
		}

		for (const FSRPlanetSurfaceGridCellId& CellId : *CellIds)
		{
			FSRPlanetSurfaceGridCell Cell;
			if (GetCellById(CellId, Cell))
			{
				AppendInteractionCell(OverlayMesh, Cell, CellColor, LineThickness);
			}
		}
	}
}

bool StarRovers::SurfaceGridInteractionOverlayBuilder::BuildInteractionOverlayMesh(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const FSRInteractionOverlayBuildInput& Input,
	FCellLookup GetCellById,
	FAppendInteractionCell AppendInteractionCell,
	FAppendInteractionRegion AppendInteractionRegion,
	FAppendInteractionPatch AppendInteractionPatch)
{
	if (!Input.bGridVisible)
	{
		return false;
	}

	AppendPreviewCells(
		OverlayMesh,
		Input.HoverGridOccupiedCellIds,
		Input.OccupiedCellColor,
		Input.DebugLineThickness * 2.0f,
		GetCellById,
		AppendInteractionCell);

	AppendPreviewCells(
		OverlayMesh,
		Input.OccupiedPreviewCellIds,
		Input.OccupiedCellColor,
		Input.DebugLineThickness * 2.5f,
		GetCellById,
		AppendInteractionCell);

	if (HasAnyCells(Input.PlacementPreviewCellIds))
	{
		AppendInteractionRegion(
			OverlayMesh,
			*Input.PlacementPreviewCellIds,
			Input.SelectedCellColor,
			Input.DebugLineThickness * 3.0f,
			false);
	}

	if (Input.AreaSelectionCellIds)
	{
		AppendInteractionRegion(
			OverlayMesh,
			*Input.AreaSelectionCellIds,
			Input.AreaSelectionCellColor,
			Input.DebugLineThickness * 2.0f,
			true);
	}

	if (Input.bIncludeCellHighlightOverlay && HasAnyCells(Input.SelectedFootprintCellIds))
	{
		AppendInteractionRegion(
			OverlayMesh,
			*Input.SelectedFootprintCellIds,
			Input.SelectedCellColor,
			Input.DebugLineThickness * 3.0f,
			false);
	}
	else if (Input.bHasSelectedCell && Input.bIncludeCellHighlightOverlay)
	{
		AppendPreviewCells(
			OverlayMesh,
			Input.SelectedHighlightCellIds,
			Input.SelectedCellColor,
			Input.DebugLineThickness * 2.5f,
			GetCellById,
			AppendInteractionCell);
	}

	if (Input.bHasHoveredCell)
	{
		FSRPlanetSurfaceGridCell HoveredCell;
		if (GetCellById(Input.HoveredCellId, HoveredCell))
		{
			if (Input.bIncludeCellHighlightOverlay && (!Input.bHasSelectedCell || !(Input.HoveredCellId == Input.SelectedCellId)))
			{
				AppendPreviewCells(
					OverlayMesh,
					Input.HoveredHighlightCellIds,
					Input.HoveredCellColor,
					Input.DebugLineThickness * 2.0f,
					GetCellById,
					AppendInteractionCell);
			}

			if (HoveredCell.bOccupied
				&& !ContainsCell(Input.ConstructionReplacementPreviewCellIds, Input.HoveredCellId))
			{
				AppendPreviewCells(
					OverlayMesh,
					Input.HoveredHighlightCellIds,
					Input.OccupiedCellColor,
					Input.DebugLineThickness * 2.5f,
					GetCellById,
					AppendInteractionCell);
			}
		}
	}

	AppendPreviewCells(
		OverlayMesh,
		Input.HoverGridInputPortCellIds,
		Input.InputPortPreviewCellColor,
		Input.DebugLineThickness * 2.5f,
		GetCellById,
		AppendInteractionCell);

	AppendPreviewCells(
		OverlayMesh,
		Input.HoverGridOutputPortCellIds,
		Input.OutputPortPreviewCellColor,
		Input.DebugLineThickness * 2.5f,
		GetCellById,
		AppendInteractionCell);

	AppendPreviewCells(
		OverlayMesh,
		Input.InputPortPreviewCellIds,
		Input.InputPortPreviewCellColor,
		Input.DebugLineThickness * 3.0f,
		GetCellById,
		AppendInteractionCell);

	AppendPreviewCells(
		OverlayMesh,
		Input.OutputPortPreviewCellIds,
		Input.OutputPortPreviewCellColor,
		Input.DebugLineThickness * 3.0f,
		GetCellById,
		AppendInteractionCell);

	if (Input.DeletionPreviewCellIds)
	{
		AppendInteractionRegion(
			OverlayMesh,
			*Input.DeletionPreviewCellIds,
			Input.DeletionPreviewCellColor,
			Input.DebugLineThickness * 3.5f,
			true);
	}

	AppendPreviewCells(
		OverlayMesh,
		Input.InvalidPreviewCellIds,
		Input.InvalidPreviewCellColor,
		Input.DebugLineThickness * 3.5f,
		GetCellById,
		AppendInteractionCell);

	if (Input.bHasHoveredCell && Input.bHoveredInteractionGridPatchVisible)
	{
		TSet<uint64> PatchDrawnEdges;
		PatchDrawnEdges.Reserve(320);
		AppendInteractionPatch(
			OverlayMesh,
			Input.HoveredCellId,
			Input.HoveredCellColor,
			Input.DebugLineThickness * 1.5f,
			PatchDrawnEdges);
	}

	return Input.bHasHoveredCell
			|| Input.bHasSelectedCell
			|| HasAnyCells(Input.SelectedFootprintCellIds)
			|| HasAnyCells(Input.PlacementPreviewCellIds)
			|| HasAnyCells(Input.AreaSelectionCellIds)
			|| HasAnyCells(Input.HoverGridOccupiedCellIds)
			|| HasAnyCells(Input.OccupiedPreviewCellIds)
			|| HasAnyCells(Input.HoverGridInputPortCellIds)
			|| HasAnyCells(Input.HoverGridOutputPortCellIds)
			|| HasAnyCells(Input.InputPortPreviewCellIds)
			|| HasAnyCells(Input.OutputPortPreviewCellIds)
			|| HasAnyCells(Input.DeletionPreviewCellIds)
			|| HasAnyCells(Input.ConstructionReplacementPreviewCellIds)
			|| HasAnyCells(Input.InvalidPreviewCellIds);
}
