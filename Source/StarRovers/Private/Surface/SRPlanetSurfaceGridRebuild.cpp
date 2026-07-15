#include "Surface/SRPlanetSurfaceGrid.h"

#include "SRPlanetSurfaceGridDefaultCells.h"
#include "SRPlanetSurfaceGridInteractionState.h"
#include "SRPlanetSurfaceGridOwnerBody.h"

namespace SurfaceGridDefaultCells = StarRovers::SurfaceGridDefaultCells;
namespace SurfaceGridInteractionState = StarRovers::SurfaceGridInteractionState;
namespace SurfaceGridOwnerBody = StarRovers::SurfaceGridOwnerBody;

void USRPlanetSurfaceGrid::RebuildDefaultSurfaceCells()
{
	bUsingGeneratedGridCells = false;
	Cells = SurfaceGridDefaultCells::BuildCubeSphereCells(
		FaceResolution,
		PlanetRadius,
		[this](const FVector& LocalUnitDirection)
		{
			return GetTerrainSampleAtDirection(LocalUnitDirection);
		},
		[](float SurfaceTemperature)
		{
			return ResolveTemperatureStateFromSurfaceTemperature(SurfaceTemperature);
		});
}

void USRPlanetSurfaceGrid::ResetSurfaceInteractionState()
{
	SurfaceGridInteractionState::ResetInteractionState(
		bHasHoveredCell,
		HoveredCellId,
		bHoveredInteractionGridPatchVisible,
		bHasSelectedCell,
		SelectedCellId,
		SelectedFootprintCellIds,
		PlacementPreviewCellIds,
		AreaSelectionCellIds,
		OccupiedPreviewCellIds,
		InputPortPreviewCellIds,
		OutputPortPreviewCellIds,
		DeletionPreviewCellIds,
		ConstructionReplacementPreviewCellIds,
		InvalidPreviewCellIds);
}

void USRPlanetSurfaceGrid::FinalizeGridRebuild()
{
	SurfaceGridOwnerBody::ClearSurfaceCellHighlights(GetOwner());
	ResetSurfaceInteractionState();
	SetInteractionOverlayVisible(false);
	RebuildCellIndex();
	RebuildCellInfoIndex();
	RebuildRaycastIndex();
	bCellsDirty = false;
	MarkGridMeshDirtyAndRefreshIfVisible();
}
