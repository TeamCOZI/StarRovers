#include "Surface/SRPlanetSurfaceGrid.h"

#include "SRPlanetSurfaceGridEmptyMesh.h"
#include "SRPlanetSurfaceGridInteractionState.h"
#include "SRPlanetSurfaceGridOwnerBody.h"
#include "SRPlanetSurfaceGridVisibilityState.h"

namespace SurfaceGridEmptyMesh = StarRovers::SurfaceGridEmptyMesh;
namespace SurfaceGridInteractionState = StarRovers::SurfaceGridInteractionState;
namespace SurfaceGridOwnerBody = StarRovers::SurfaceGridOwnerBody;
namespace SurfaceGridVisibilityState = StarRovers::SurfaceGridVisibilityState;

void USRPlanetSurfaceGrid::ApplyGridVisibilityState()
{
	SurfaceGridVisibilityState::HidePrimaryGridComponent(*this);
	SetInteractionOverlayVisible(ShouldShowInteractionOverlayForCurrentState());

	if (bGridVisible)
	{
		PrepareGridForVisibleState();
	}
	else
	{
		ClearInteractionStateForHiddenGrid();
	}
}

bool USRPlanetSurfaceGrid::ShouldShowInteractionOverlayForCurrentState() const
{
	return bGridVisible && SurfaceGridInteractionState::HasInteractionOverlayContent(
		bHasHoveredCell,
		bHasSelectedCell,
		SelectedFootprintCellIds,
		PlacementPreviewCellIds,
		AreaSelectionCellIds,
		OccupiedPreviewCellIds,
		InputPortPreviewCellIds,
		OutputPortPreviewCellIds,
		HoverGridOccupiedCellIds,
		HoverGridInputPortCellIds,
		HoverGridOutputPortCellIds,
		DeletionPreviewCellIds,
		ConstructionReplacementPreviewCellIds,
		InvalidPreviewCellIds);
}

void USRPlanetSurfaceGrid::ClearInteractionStateForHiddenGrid()
{
	ClearHoveredCell();
	ClearSelectedCell();
	ClearSelectedFootprintCells();
	ClearPlacementPreviewCells();
	ClearAreaSelectionCells();
	ClearHoverGridHighlightCells();
	ClearOccupiedPreviewCells();
	ClearFacilityPortPreviewCells();
	ClearDeletionPreviewCells();
	ClearConstructionReplacementPreviewCells();
	ClearInvalidPreviewCells();
}

void USRPlanetSurfaceGrid::PrepareGridForVisibleState()
{
	SurfaceGridOwnerBody::PrepareDynamicMesh(GetOwner());

	if (bCellsDirty)
	{
		RebuildGrid();
	}

	RequestInteractionHighlightRefresh();
}

void USRPlanetSurfaceGrid::EnsureAssemblyGridCellsReady()
{
	if (Cells.IsEmpty() || bCellsDirty)
	{
		RebuildGrid();
	}
}

void USRPlanetSurfaceGrid::ApplyEmptyPrimaryGridMeshIfNeeded()
{
	if (!bCellsDirty && bGridMeshDirty)
	{
		SurfaceGridEmptyMesh::ApplyEmptyPrimaryColorMesh(*this);
		bGridMeshDirty = false;
	}
}
