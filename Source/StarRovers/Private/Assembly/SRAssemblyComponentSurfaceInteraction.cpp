#include "Assembly/SRAssemblyComponent.h"

#include "Assembly/SRAssemblySurfaceCursorQuery.h"
#include "Assembly/SRAssemblySurfaceHoverUpdater.h"
#include "Camera/SRPlayerController.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

void USRAssemblyComponent::ClearSurfaceGridInteraction(AActor* SurfaceActor)
{
	USRPlanetSurfaceGrid* CurrentSurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SurfaceActor);
	if (CurrentSurfaceGrid)
	{
		CurrentSurfaceGrid->ClearHoveredCell();
		CurrentSurfaceGrid->ClearHoverGridHighlightCells();
		CurrentSurfaceGrid->ClearSelectedCell();
		CurrentSurfaceGrid->ClearSelectedFootprintCells();
		CurrentSurfaceGrid->ClearPlacementPreviewCells();
		CurrentSurfaceGrid->ClearOccupiedPreviewCells();
		CurrentSurfaceGrid->ClearFacilityPortPreviewCells();
		CurrentSurfaceGrid->ClearDeletionPreviewCells();
		CurrentSurfaceGrid->ClearConstructionReplacementPreviewCells();
		CurrentSurfaceGrid->ClearInvalidPreviewCells();
		CurrentSurfaceGrid->SetGridVisible(false);
	}
	if (CurrentSurfaceGrid == SurfaceState.ActiveAssemblySurfaceGrid)
	{
		SurfaceState.ActiveAssemblySurfaceGrid = nullptr;
	}

	ConveyorPreview.ClearPortPreview();
	ConveyorPreview.ClearBulkDeletionPreview();
	if (!IsValid(SurfaceActor) || CurrentSurfaceGrid == HoveredSurfaceGrid)
	{
		HoveredSurfaceGrid = nullptr;
	}
	ClearPublishedHoveredCellInfo();
	ClearSelectedStructureInfo();
	ResetHoverSampleCache();
	EndStructurePlacementDrag();
	ClearAreaSelection();
	ClearAreaDeletion();
	CancelAreaCopyPlacement();
	ClearPendingConveyorPathStart();
	PlacementQueue.Reset();
	StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
	ConveyorPreview.DestroyGhostActor(HoveredSurfaceGrid);
	ConveyorPreview.DestroyDeletionGhostActor();
}

void USRAssemblyComponent::ClearSurfaceHover()
{
	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->ClearHoveredCell();
		HoveredSurfaceGrid->ClearHoverGridHighlightCells();
		HoveredSurfaceGrid->ClearSelectedFootprintCells();
		HoveredSurfaceGrid->ClearPlacementPreviewCells();
		HoveredSurfaceGrid->ClearOccupiedPreviewCells();
		HoveredSurfaceGrid->ClearFacilityPortPreviewCells();
		HoveredSurfaceGrid->ClearDeletionPreviewCells();
		HoveredSurfaceGrid->ClearConstructionReplacementPreviewCells();
		HoveredSurfaceGrid->ClearInvalidPreviewCells();
	}

	ConveyorPreview.ClearPortPreview();
	ConveyorPreview.ClearBulkDeletionPreview();
	HoveredSurfaceGrid = nullptr;
	ClearPublishedHoveredCellInfo();
	ClearSelectedStructureInfo();
	ResetHoverSampleCache();
	EndStructurePlacementDrag();
	ClearAreaSelection();
	ClearAreaDeletion();
	ClearPendingConveyorPathStart();
	PlacementQueue.Reset();
	StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
	ConveyorPreview.DestroyGhostActor(HoveredSurfaceGrid);
	ConveyorPreview.DestroyDeletionGhostActor();
}

void USRAssemblyComponent::ClearSurfaceHoverPreview()
{
	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->ClearHoveredCell();
		HoveredSurfaceGrid->ClearHoverGridHighlightCells();
		HoveredSurfaceGrid->ClearConstructionReplacementPreviewCells();
	}

	ConveyorPreview.ClearPortPreview();
	ConveyorPreview.ClearBulkDeletionPreview();
	ConveyorPreview.ClearInvalidPlacementPreview();
	HoveredSurfaceGrid = nullptr;
	ClearPublishedHoveredCellInfo();
	ResetHoverSampleCache();
	StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
	ConveyorPreview.DestroyGhostActor(HoveredSurfaceGrid);
	ConveyorPreview.DestroyDeletionGhostActor();
}

void USRAssemblyComponent::UpdateSurfaceHover()
{
	StarRovers::Assembly::FSRAssemblySurfaceHoverUpdate HoverUpdate;
	USRPlanetSurfaceGrid* HoveredSurfaceGridRaw = HoveredSurfaceGrid.Get();
	const StarRovers::Assembly::ESRAssemblySurfaceHoverUpdateResult UpdateResult =
		StarRovers::Assembly::FSRAssemblySurfaceHoverUpdater::Update(
			GetOwnerController(),
			ModeState.bAssemblyModeActive,
			HoveredSurfaceGridRaw,
			SurfaceState,
			HoverUpdate);
	HoveredSurfaceGrid = HoveredSurfaceGridRaw;
	if (UpdateResult == StarRovers::Assembly::ESRAssemblySurfaceHoverUpdateResult::ClearHover)
	{
		ClearSurfaceHover();
		return;
	}
	if (UpdateResult == StarRovers::Assembly::ESRAssemblySurfaceHoverUpdateResult::ClearHoverPreview)
	{
		ClearSurfaceHoverPreview();
		return;
	}
	if (UpdateResult == StarRovers::Assembly::ESRAssemblySurfaceHoverUpdateResult::Updated)
	{
		PublishHoveredCellInfo(HoverUpdate.SurfaceGrid, HoverUpdate.HoveredCell);
	}
}

void USRAssemblyComponent::ApplyAssemblyModeToFocusedSurfaceGrid()
{
	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
	const bool bHasFocusedSurfaceGrid = StarRovers::Assembly::FSRAssemblySurfaceCursorQuery::TryGetFocusedSurfaceGrid(
		GetOwnerController(),
		FocusedActor,
		FocusedSurfaceGrid);
	USRPlanetSurfaceGrid* DesiredSurfaceGrid = ModeState.bAssemblyModeActive && bHasFocusedSurfaceGrid ? FocusedSurfaceGrid : nullptr;

	if (SurfaceState.ActiveAssemblySurfaceGrid && SurfaceState.ActiveAssemblySurfaceGrid != DesiredSurfaceGrid)
	{
		ClearAreaSelection();
		ClearAreaDeletion();
		CancelAreaCopyPlacement();
		SurfaceState.ActiveAssemblySurfaceGrid->SetGridVisible(false);
		SurfaceState.ActiveAssemblySurfaceGrid->ClearSelectedFootprintCells();
		SurfaceState.ActiveAssemblySurfaceGrid->ClearPlacementPreviewCells();
		SurfaceState.ActiveAssemblySurfaceGrid->ClearOccupiedPreviewCells();
		SurfaceState.ActiveAssemblySurfaceGrid->ClearFacilityPortPreviewCells();
		SurfaceState.ActiveAssemblySurfaceGrid->ClearDeletionPreviewCells();
		SurfaceState.ActiveAssemblySurfaceGrid->ClearConstructionReplacementPreviewCells();
		SurfaceState.ActiveAssemblySurfaceGrid->ClearInvalidPreviewCells();
		ConveyorPreview.ClearPortPreview();
		ConveyorPreview.ClearBulkDeletionPreview();
	}

	SurfaceState.ActiveAssemblySurfaceGrid = DesiredSurfaceGrid;
	if (SurfaceState.ActiveAssemblySurfaceGrid)
	{
		SurfaceState.ActiveAssemblySurfaceGrid->SetGridVisible(true);
	}

	if (!ModeState.bAssemblyModeActive)
	{
		ClearSurfaceHover();
		StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
		ConveyorPreview.DestroyGhostActor(HoveredSurfaceGrid);
		ConveyorPreview.DestroyDeletionGhostActor();
		ConveyorPreview.ClearPortPreview();
		ConveyorPreview.ClearBulkDeletionPreview();
		if (SurfaceState.ActiveAssemblySurfaceGrid)
		{
			SurfaceState.ActiveAssemblySurfaceGrid->ClearSelectedFootprintCells();
			SurfaceState.ActiveAssemblySurfaceGrid->ClearPlacementPreviewCells();
			SurfaceState.ActiveAssemblySurfaceGrid->ClearOccupiedPreviewCells();
			SurfaceState.ActiveAssemblySurfaceGrid->ClearFacilityPortPreviewCells();
			SurfaceState.ActiveAssemblySurfaceGrid->ClearDeletionPreviewCells();
			SurfaceState.ActiveAssemblySurfaceGrid->ClearConstructionReplacementPreviewCells();
			SurfaceState.ActiveAssemblySurfaceGrid->ClearInvalidPreviewCells();
		}
	}
}
