#include "Assembly/SRAssemblyComponent.h"

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
		CurrentSurfaceGrid->ClearSelectedCell();
		CurrentSurfaceGrid->ClearFacilityPortPreviewCells();
		CurrentSurfaceGrid->SetGridVisible(false);
	}
	if (CurrentSurfaceGrid == ActiveAssemblySurfaceGrid)
	{
		ActiveAssemblySurfaceGrid = nullptr;
	}

	if (!IsValid(SurfaceActor) || CurrentSurfaceGrid == HoveredSurfaceGrid)
	{
		HoveredSurfaceGrid = nullptr;
	}
	ClearPublishedHoveredCellInfo();
	ClearSelectedStructureInfo();
	ResetHoverSampleCache();
	EndStructurePlacementDrag();
	ClearPendingConveyorPathStart();
	PendingStructurePlacementQueue.Reset();
	DestroyStructureGhostPreview();
}

void USRAssemblyComponent::ClearSurfaceHover()
{
	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->ClearHoveredCell();
		HoveredSurfaceGrid->ClearFacilityPortPreviewCells();
	}

	HoveredSurfaceGrid = nullptr;
	ClearPublishedHoveredCellInfo();
	ClearSelectedStructureInfo();
	ResetHoverSampleCache();
	EndStructurePlacementDrag();
	ClearPendingConveyorPathStart();
	PendingStructurePlacementQueue.Reset();
	DestroyStructureGhostPreview();
}

void USRAssemblyComponent::ClearSurfaceHoverPreview()
{
	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->ClearHoveredCell();
	}

	HoveredSurfaceGrid = nullptr;
	ClearPublishedHoveredCellInfo();
	ResetHoverSampleCache();
	DestroyStructureGhostPreview();
}

void USRAssemblyComponent::UpdateSurfaceHover()
{
	if (!bAssemblyModeActive)
	{
		ClearSurfaceHover();
		return;
	}

	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController)
	{
		ClearSurfaceHover();
		return;
	}

	if (PlayerController->IsPointerOverBlockingUi())
	{
		ClearSurfaceHoverPreview();
		return;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	if (!TryGetFocusedSurfaceGrid(FocusedActor, SurfaceGrid))
	{
		ClearSurfaceHover();
		return;
	}

	FVector2D CurrentMousePosition = FVector2D::ZeroVector;
	const bool bHasMousePosition = PlayerController->GetMousePosition(CurrentMousePosition.X, CurrentMousePosition.Y);
	if (bHasMousePosition
		&& bHasLastHoveredSampleMousePosition
		&& LastHoveredSampleSurfaceGrid == SurfaceGrid
		&& HoveredSurfaceGrid == SurfaceGrid
		&& SurfaceGrid->HasHoveredCell()
		&& FVector2D::Distance(CurrentMousePosition, LastHoveredSampleMousePosition) <= 0.5f)
	{
		return;
	}

	FSRPlanetSurfaceGridCell HoveredCell;
	FVector HoverHitLocation = FVector::ZeroVector;
	if (!TryProjectCursorToSurfaceCell(SurfaceGrid, HoveredCell, HoverHitLocation))
	{
		ClearSurfaceHover();
		return;
	}

	if (HoveredSurfaceGrid && HoveredSurfaceGrid != SurfaceGrid)
	{
		HoveredSurfaceGrid->ClearHoveredCell();
	}

	HoveredSurfaceGrid = SurfaceGrid;
	HoveredSurfaceGrid->SetHoveredCell(HoveredCell.CellId);
	PublishHoveredCellInfo(SurfaceGrid, HoveredCell);
	if (bHasMousePosition)
	{
		LastHoveredSampleSurfaceGrid = SurfaceGrid;
		LastHoveredSampleMousePosition = CurrentMousePosition;
		bHasLastHoveredSampleMousePosition = true;
	}
}

void USRAssemblyComponent::ApplyAssemblyModeToFocusedSurfaceGrid()
{
	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
	const bool bHasFocusedSurfaceGrid = TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid);
	USRPlanetSurfaceGrid* DesiredSurfaceGrid = bAssemblyModeActive && bHasFocusedSurfaceGrid ? FocusedSurfaceGrid : nullptr;

	if (ActiveAssemblySurfaceGrid && ActiveAssemblySurfaceGrid != DesiredSurfaceGrid)
	{
		ActiveAssemblySurfaceGrid->SetGridVisible(false);
		ActiveAssemblySurfaceGrid->ClearFacilityPortPreviewCells();
	}

	ActiveAssemblySurfaceGrid = DesiredSurfaceGrid;
	if (ActiveAssemblySurfaceGrid)
	{
		ActiveAssemblySurfaceGrid->SetGridVisible(true);
	}

	if (!bAssemblyModeActive)
	{
		ClearSurfaceHover();
		DestroyStructureGhostPreview();
		if (ActiveAssemblySurfaceGrid)
		{
			ActiveAssemblySurfaceGrid->ClearFacilityPortPreviewCells();
		}
	}
}
