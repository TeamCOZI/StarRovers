#include "Surface/SRPlanetSurfaceGrid.h"

#include "SRPlanetSurfaceGridCellIndex.h"
#include "SRPlanetSurfaceGridGeneratedGridFinalize.h"
#include "SRPlanetSurfaceGridGeneratedGridState.h"
#include "SRPlanetSurfaceGridOwnerBody.h"

namespace SurfaceGridCellIndex = StarRovers::SurfaceGridCellIndex;
namespace SurfaceGridGeneratedGridFinalize = StarRovers::SurfaceGridGeneratedGridFinalize;
namespace SurfaceGridGeneratedGridState = StarRovers::SurfaceGridGeneratedGridState;
namespace SurfaceGridOwnerBody = StarRovers::SurfaceGridOwnerBody;

void USRPlanetSurfaceGrid::ClearGeneratedGridBuildHighlights()
{
	SurfaceGridOwnerBody::ClearSurfaceCellHighlights(GetOwner());
}

void USRPlanetSurfaceGrid::AssignGeneratedGridBuildCells(TArray<FSRPlanetSurfaceGridCell>&& NewCells)
{
	SurfaceGridGeneratedGridState::AssignGeneratedCells(Cells, MoveTemp(NewCells), FaceResolution, bUsingGeneratedGridCells);
	SurfaceGridGeneratedGridState::ResetGeneratedGridInteractionState(
		bHasHoveredCell,
		HoveredCellId,
		bHasSelectedCell,
		SelectedCellId,
		InputPortPreviewCellIds,
		OutputPortPreviewCellIds,
		DeletionPreviewCellIds,
		InvalidPreviewCellIds);
	SetInteractionOverlayVisible(false);
}

void USRPlanetSurfaceGrid::ApplyGeneratedGridCellIndex(TArray<int32>&& NewCellIndexByFlatId)
{
	if (!SurfaceGridCellIndex::TryAssignFlatCellIndex(CellIndexState, MoveTemp(NewCellIndexByFlatId), FaceResolution))
	{
		RebuildCellIndex();
	}
}

void USRPlanetSurfaceGrid::RebuildGeneratedGridCellInfoIndex()
{
	RebuildCellInfoIndex();
}

void USRPlanetSurfaceGrid::RebuildGeneratedGridRaycastIndex()
{
	RebuildRaycastIndex();
}

void USRPlanetSurfaceGrid::FinalizeGeneratedGridBuildMesh()
{
	SurfaceGridGeneratedGridFinalize::FinalizeGeneratedGridMesh(
		*this,
		bCellsDirty,
		bGridMeshDirty,
		[this]()
		{
			UpdateDebugTickState();
		});
}
