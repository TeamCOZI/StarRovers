#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRPlayerController.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	FString FormatSurfaceHoverCellId(const FSRPlanetSurfaceGridCellId& CellId)
	{
		return FString::Printf(
			TEXT("Face=%d X=%d Y=%d"),
			static_cast<int32>(CellId.Face),
			CellId.CellX,
			CellId.CellY);
	}
}

void USRAssemblyComponent::ClearSurfaceGridInteraction(AActor* SurfaceActor)
{
	USRPlanetSurfaceGrid* CurrentSurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SurfaceActor);
	if (CurrentSurfaceGrid)
	{
		CurrentSurfaceGrid->ClearHoveredCell();
		CurrentSurfaceGrid->ClearSelectedCell();
		CurrentSurfaceGrid->ClearOccupiedPreviewCells();
		CurrentSurfaceGrid->ClearFacilityPortPreviewCells();
		CurrentSurfaceGrid->ClearDeletionPreviewCells();
		CurrentSurfaceGrid->ClearInvalidPreviewCells();
		CurrentSurfaceGrid->SetGridVisible(false);
	}
	if (CurrentSurfaceGrid == ActiveAssemblySurfaceGrid)
	{
		ActiveAssemblySurfaceGrid = nullptr;
	}

	ClearConveyorPlacementPortPreview();
	ClearConveyorBulkDeletionPreview();
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
	PendingStructurePlacementQueue.Reset();
	DestroyStructureGhostPreview();
	DestroyConveyorGhostPreview();
	DestroyConveyorDeletionGhostPreview();
}

void USRAssemblyComponent::ClearSurfaceHover()
{
	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->ClearHoveredCell();
		HoveredSurfaceGrid->ClearOccupiedPreviewCells();
		HoveredSurfaceGrid->ClearFacilityPortPreviewCells();
		HoveredSurfaceGrid->ClearDeletionPreviewCells();
		HoveredSurfaceGrid->ClearInvalidPreviewCells();
	}

	ClearConveyorPlacementPortPreview();
	ClearConveyorBulkDeletionPreview();
	HoveredSurfaceGrid = nullptr;
	ClearPublishedHoveredCellInfo();
	ClearSelectedStructureInfo();
	ResetHoverSampleCache();
	EndStructurePlacementDrag();
	ClearAreaSelection();
	ClearAreaDeletion();
	ClearPendingConveyorPathStart();
	PendingStructurePlacementQueue.Reset();
	DestroyStructureGhostPreview();
	DestroyConveyorGhostPreview();
	DestroyConveyorDeletionGhostPreview();
}

void USRAssemblyComponent::ClearSurfaceHoverPreview()
{
	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->ClearHoveredCell();
	}

	ClearConveyorPlacementPortPreview();
	ClearConveyorBulkDeletionPreview();
	ClearConveyorInvalidPlacementPreview();
	HoveredSurfaceGrid = nullptr;
	ClearPublishedHoveredCellInfo();
	ResetHoverSampleCache();
	DestroyStructureGhostPreview();
	DestroyConveyorGhostPreview();
	DestroyConveyorDeletionGhostPreview();
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

	SurfaceGrid->SetHoveredInteractionGridPatchVisible(IsValid(PlayerController->GetSelectedStructureDataAsset()));

	FVector2D CurrentMousePosition = FVector2D::ZeroVector;
	const bool bHasMousePosition = PlayerController->GetMousePosition(CurrentMousePosition.X, CurrentMousePosition.Y);
	if (bHasMousePosition
		&& bHasLastHoveredSampleMousePosition
		&& LastHoveredSampleSurfaceGrid == SurfaceGrid
		&& HoveredSurfaceGrid == SurfaceGrid
		&& SurfaceGrid->HasHoveredCell()
		&& FVector2D::Distance(CurrentMousePosition, LastHoveredSampleMousePosition) <= 0.5f)
	{
		FSRPlanetSurfaceGridCell CachedHoveredCell;
		if (bHasLastPublishedHoveredCellInfo
			&& SurfaceGrid->GetHoveredCell(CachedHoveredCell)
			&& !(CachedHoveredCell.CellId == LastPublishedHoveredCellId))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[SR SurfaceHover] Result=CacheMismatch GridHovered={%s} PublishedHovered={%s} MouseDelta=%.3f"),
				*FormatSurfaceHoverCellId(CachedHoveredCell.CellId),
				*FormatSurfaceHoverCellId(LastPublishedHoveredCellId),
				FVector2D::Distance(CurrentMousePosition, LastHoveredSampleMousePosition));
		}
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
	FSRPlanetSurfaceGridCell PreviousHoveredCell;
	const bool bHadPreviousHoveredCell = SurfaceGrid->GetHoveredCell(PreviousHoveredCell);
	HoveredSurfaceGrid->SetHoveredCell(HoveredCell.CellId);
	PublishHoveredCellInfo(SurfaceGrid, HoveredCell);
	if (bHadPreviousHoveredCell && PreviousHoveredCell.CellId.Face != HoveredCell.CellId.Face)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SR SurfaceHover] Result=FaceTransition Previous={%s} Current={%s} Hit=%s"),
			*FormatSurfaceHoverCellId(PreviousHoveredCell.CellId),
			*FormatSurfaceHoverCellId(HoveredCell.CellId),
			*HoverHitLocation.ToCompactString());
	}
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
		ClearAreaSelection();
		ClearAreaDeletion();
		CancelAreaCopyPlacement();
		ActiveAssemblySurfaceGrid->SetGridVisible(false);
		ActiveAssemblySurfaceGrid->ClearOccupiedPreviewCells();
		ActiveAssemblySurfaceGrid->ClearFacilityPortPreviewCells();
		ActiveAssemblySurfaceGrid->ClearDeletionPreviewCells();
		ActiveAssemblySurfaceGrid->ClearInvalidPreviewCells();
		ClearConveyorPlacementPortPreview();
		ClearConveyorBulkDeletionPreview();
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
		DestroyConveyorGhostPreview();
		DestroyConveyorDeletionGhostPreview();
		ClearConveyorPlacementPortPreview();
		ClearConveyorBulkDeletionPreview();
		if (ActiveAssemblySurfaceGrid)
		{
			ActiveAssemblySurfaceGrid->ClearOccupiedPreviewCells();
			ActiveAssemblySurfaceGrid->ClearFacilityPortPreviewCells();
			ActiveAssemblySurfaceGrid->ClearDeletionPreviewCells();
			ActiveAssemblySurfaceGrid->ClearInvalidPreviewCells();
		}
	}
}
