#include "Surface/SRPlanetSurfaceGrid.h"

#include "SRPlanetSurfaceGridInteractionCellBuilder.h"
#include "SRPlanetSurfaceGridInteractionOverlayComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "SRPlanetSurfaceGridInteractionOverlayBuilder.h"
#include "SRPlanetSurfaceGridInteractionPatchOverlay.h"
#include "SRPlanetSurfaceGridInteractionRegionBuilder.h"
#include "SRPlanetSurfaceGridOwnerBody.h"

namespace SurfaceGridInteractionCellBuilder = StarRovers::SurfaceGridInteractionCellBuilder;
namespace SurfaceGridInteractionOverlayBuilder = StarRovers::SurfaceGridInteractionOverlayBuilder;
namespace SurfaceGridInteractionOverlayComponent = StarRovers::SurfaceGridInteractionOverlayComponent;
namespace SurfaceGridInteractionPatchOverlay = StarRovers::SurfaceGridInteractionPatchOverlay;
namespace SurfaceGridInteractionRegionBuilder = StarRovers::SurfaceGridInteractionRegionBuilder;
namespace SurfaceGridOwnerBody = StarRovers::SurfaceGridOwnerBody;

void USRPlanetSurfaceGrid::EnsureInteractionOverlay()
{
	if (IsTemplate())
	{
		return;
	}

	InteractionOverlayMesh = SurfaceGridInteractionOverlayComponent::EnsureInteractionOverlay(
		InteractionOverlayMesh,
		this,
		GetOwner(),
		GridOverlayMaterial.Get(),
		GetMaterial(0));
}

void USRPlanetSurfaceGrid::RequestInteractionHighlightRefresh()
{
	if (InteractionBatch.IsActive())
	{
		InteractionBatch.MarkHighlightRefreshPending();
		return;
	}

	RefreshInteractionHighlight();
}

void USRPlanetSurfaceGrid::RefreshInteractionHighlight()
{
	const bool bUseDynamicMeshHighlight = !bGridVisible;
	const bool bAppliedDynamicMeshHighlight = bUseDynamicMeshHighlight
		&& SurfaceGridOwnerBody::ApplySurfaceCellHighlights(
			GetOwner(),
			HoveredCellId,
			bHasHoveredCell,
			SelectedCellId,
			bHasSelectedCell,
			HoveredCellColor,
			SelectedCellColor);

	if (!bUseDynamicMeshHighlight || (!bHasHoveredCell && !bHasSelectedCell))
	{
		SurfaceGridOwnerBody::ClearSurfaceCellHighlights(GetOwner());
	}

	RebuildInteractionOverlayMesh(!bAppliedDynamicMeshHighlight);
}

void USRPlanetSurfaceGrid::RebuildInteractionOverlayMesh(bool bIncludeCellHighlightOverlay)
{
	EnsureInteractionOverlay();
	if (!InteractionOverlayMesh)
	{
		return;
	}

	UE::Geometry::FDynamicMesh3 OverlayMesh;
	OverlayMesh.EnableAttributes();
	OverlayMesh.Attributes()->EnablePrimaryColors();

	SurfaceGridInteractionOverlayBuilder::FSRInteractionOverlayBuildInput BuildInput;
	BuildInput.bGridVisible = bGridVisible;
	BuildInput.bIncludeCellHighlightOverlay = bIncludeCellHighlightOverlay;
	BuildInput.bHasHoveredCell = bHasHoveredCell;
	BuildInput.bHasSelectedCell = bHasSelectedCell;
	BuildInput.bHoveredInteractionGridPatchVisible = bHoveredInteractionGridPatchVisible;
	BuildInput.HoveredCellId = HoveredCellId;
	BuildInput.SelectedCellId = SelectedCellId;
	BuildInput.HoveredCellColor = HoveredCellColor;
	BuildInput.SelectedCellColor = SelectedCellColor;
	BuildInput.OccupiedCellColor = OccupiedCellColor;
	BuildInput.AreaSelectionCellColor = AreaSelectionCellColor;
	BuildInput.InputPortPreviewCellColor = InputPortPreviewCellColor;
	BuildInput.OutputPortPreviewCellColor = OutputPortPreviewCellColor;
	BuildInput.DeletionPreviewCellColor = DeletionPreviewCellColor;
	BuildInput.InvalidPreviewCellColor = InvalidPreviewCellColor;
	BuildInput.DebugLineThickness = DebugLineThickness;
	BuildInput.AreaSelectionCellIds = &AreaSelectionCellIds;
	BuildInput.InputPortPreviewCellIds = &InputPortPreviewCellIds;
	BuildInput.OutputPortPreviewCellIds = &OutputPortPreviewCellIds;
	BuildInput.OccupiedPreviewCellIds = &OccupiedPreviewCellIds;
	BuildInput.DeletionPreviewCellIds = &DeletionPreviewCellIds;
	BuildInput.InvalidPreviewCellIds = &InvalidPreviewCellIds;

	const bool bShouldShowInteractionOverlay = SurfaceGridInteractionOverlayBuilder::BuildInteractionOverlayMesh(
		OverlayMesh,
		BuildInput,
		[this](const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell)
		{
			return GetCellById(CellId, OutCell);
		},
		[this](UE::Geometry::FDynamicMesh3& Mesh, const FSRPlanetSurfaceGridCell& Cell, const FLinearColor& LineColor, float LineThickness)
		{
			AppendInteractionCell(Mesh, Cell, LineColor, LineThickness);
		},
		[this](UE::Geometry::FDynamicMesh3& Mesh, const TArray<FSRPlanetSurfaceGridCellId>& CellIds, const FLinearColor& LineColor, float LineThickness, bool bPreferCompactRectangles)
		{
			AppendInteractionCellRegion(Mesh, CellIds, LineColor, LineThickness, bPreferCompactRectangles);
		},
		[this](UE::Geometry::FDynamicMesh3& Mesh, const FSRPlanetSurfaceGridCellId& CenterCellId, const FLinearColor& BaseLineColor, float LineThickness, TSet<uint64>& DrawnEdges)
		{
			AppendInteractionGridPatch(Mesh, CenterCellId, BaseLineColor, LineThickness, DrawnEdges);
		});

	InteractionOverlayMesh->SetMesh(MoveTemp(OverlayMesh));
	SetInteractionOverlayVisible(bShouldShowInteractionOverlay);
}

void USRPlanetSurfaceGrid::AppendInteractionGridPatch(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const FSRPlanetSurfaceGridCellId& CenterCellId,
	const FLinearColor& BaseLineColor,
	float LineThickness,
	TSet<uint64>& DrawnEdges) const
{
	SurfaceGridInteractionPatchOverlay::AppendInteractionGridPatch(
		OverlayMesh,
		CenterCellId,
		BaseLineColor,
		LineThickness,
		DebugLineOpacity,
		DrawnEdges,
		[this](const FSRPlanetSurfaceGridCellId& PatchCenterCellId, TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
		{
			return GetInteractionGridPatchCellIds(PatchCenterCellId, OutCellIds);
		},
		[this](UE::Geometry::FDynamicMesh3& Mesh, const TArray<FSRPlanetSurfaceGridCellId>& PatchCellIds, const FLinearColor& PatchLineColor, float PatchLineThickness, TSet<uint64>& PatchDrawnEdges)
		{
			AppendInteractionCellRegionBoundary(Mesh, PatchCellIds, PatchLineColor, PatchLineThickness, false, &PatchDrawnEdges);
		});
}

void USRPlanetSurfaceGrid::SetInteractionOverlayVisible(bool bNewVisible)
{
	SurfaceGridInteractionOverlayComponent::SetInteractionOverlayVisible(InteractionOverlayMesh, bNewVisible);
}

void USRPlanetSurfaceGrid::AppendInteractionCell(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const FSRPlanetSurfaceGridCell& Cell,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	SurfaceGridInteractionCellBuilder::AppendInteractionCell(
		OverlayMesh,
		Cell,
		LineColor,
		LineThickness,
		bUsingGeneratedGridCells,
		GridSurfaceOffset,
		[this](const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell)
		{
			return GetCellById(CellId, OutCell);
		},
		[this](const FVector& LocalUnitDirection, float HeightOffset)
		{
			return ResolveLocalSurfacePoint(LocalUnitDirection, HeightOffset);
		});
}

void USRPlanetSurfaceGrid::AppendInteractionCellRegionBoundary(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	const FLinearColor& LineColor,
	float LineThickness,
	bool bIncludeFill,
	TSet<uint64>* SharedDrawnEdges) const
{
	SurfaceGridInteractionRegionBuilder::AppendInteractionCellRegionBoundary(
		OverlayMesh,
		CellIds,
		LineColor,
		LineThickness,
		bIncludeFill,
		SharedDrawnEdges,
		bUsingGeneratedGridCells,
		GridSurfaceOffset,
		[this](const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell)
		{
			return GetCellById(CellId, OutCell);
		},
		[this](const FVector& LocalUnitDirection, float HeightOffset)
		{
			return ResolveLocalSurfacePoint(LocalUnitDirection, HeightOffset);
		});
}

void USRPlanetSurfaceGrid::AppendInteractionCellRegion(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	const FLinearColor& LineColor,
	float LineThickness,
	bool bPreferCompactRectangles) const
{
	SurfaceGridInteractionRegionBuilder::AppendInteractionCellRegion(
		OverlayMesh,
		CellIds,
		LineColor,
		LineThickness,
		bPreferCompactRectangles,
		bUsingGeneratedGridCells,
		GridSurfaceOffset,
		[this](const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell)
		{
			return GetCellById(CellId, OutCell);
		},
		[this](const FVector& LocalUnitDirection, float HeightOffset)
		{
			return ResolveLocalSurfacePoint(LocalUnitDirection, HeightOffset);
		});
}

bool USRPlanetSurfaceGrid::TryAppendRectangularInteractionCellRegion(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	return SurfaceGridInteractionRegionBuilder::TryAppendRectangularInteractionCellRegion(
		OverlayMesh,
		CellIds,
		LineColor,
		LineThickness,
		bUsingGeneratedGridCells,
		GridSurfaceOffset,
		[this](const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell)
		{
			return GetCellById(CellId, OutCell);
		},
		[this](const FVector& LocalUnitDirection, float HeightOffset)
		{
			return ResolveLocalSurfacePoint(LocalUnitDirection, HeightOffset);
		});
}
