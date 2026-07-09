#include "Assembly/SRAssemblyComponent.h"

#include "Assembly/SRAssemblySurfaceCursorQuery.h"
#include "Camera/SRPlayerController.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool USRAssemblyComponent::ShouldHandleAreaSelectionDrag() const
{
	const ASRPlayerController* PlayerController = GetOwnerController();
	if (!ModeState.bAssemblyModeActive
		|| AreaCopy.IsPlacementActive()
		|| !IsValid(PlayerController)
		|| PlayerController->IsPointerOverBlockingUI())
	{
		return false;
	}

	return PlayerController->IsAssemblyShiftModifierActive();
}

bool USRAssemblyComponent::BeginAreaSelectionDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!ShouldHandleAreaSelectionDrag())
	{
		return false;
	}

	EndStructurePlacementDrag(false);
	ClearAreaSelection();
	ClearAreaDeletion();

	const ASRPlayerController* PlayerController = GetOwnerController();
	StarRovers::Assembly::FSRAssemblySurfaceCursorTarget CursorTarget;
	if (!StarRovers::Assembly::FSRAssemblySurfaceCursorQuery::TryResolveSurfaceCell(PlayerController, CursorTarget))
	{
		return false;
	}

	AreaSelection.BeginSelectionDrag(CursorTarget.SurfaceGrid, CursorTarget.Cell.CellId);
	OutSelectedActor = CursorTarget.FocusedActor;
	return AreaSelection.UpdateSelectionPreview(CursorTarget.SurfaceGrid, CursorTarget.Cell.CellId);
}

bool USRAssemblyComponent::ContinueAreaSelectionDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!AreaSelection.IsSelectionDragActive() || !AreaSelection.HasSelectionStartCell())
	{
		return false;
	}

	const ASRPlayerController* PlayerController = GetOwnerController();
	StarRovers::Assembly::FSRAssemblySurfaceCursorTarget CursorTarget;
	if (!StarRovers::Assembly::FSRAssemblySurfaceCursorQuery::TryResolveSurfaceCell(PlayerController, CursorTarget)
		|| CursorTarget.SurfaceGrid != AreaSelection.GetSelectionSurfaceGrid())
	{
		return false;
	}

	OutSelectedActor = CursorTarget.FocusedActor;
	return AreaSelection.UpdateSelectionPreview(CursorTarget.SurfaceGrid, CursorTarget.Cell.CellId);
}

void USRAssemblyComponent::EndAreaSelectionDrag()
{
	AreaSelection.EndSelectionDrag();
}

void USRAssemblyComponent::ClearAreaSelection()
{
	AreaSelection.ClearSelectionPreview();
	AreaSelection.ClearSelection();
}

bool USRAssemblyComponent::TryDeleteAreaSelection()
{
	USRPlanetSurfaceGrid* SelectionSurfaceGrid = AreaSelection.GetSelectionSurfaceGrid();
	const TArray<FSRPlanetSurfaceGridCellId>& SelectionCellIds = AreaSelection.GetSelectionCellIds();
	if (!ModeState.bAssemblyModeActive || !IsValid(SelectionSurfaceGrid) || SelectionCellIds.IsEmpty())
	{
		return false;
	}

	if (!DeleteAreaCells(SelectionSurfaceGrid, SelectionCellIds))
	{
		return false;
	}

	ClearAreaSelection();
	return true;
}

bool USRAssemblyComponent::ShouldHandleAreaDeletionDrag() const
{
	const ASRPlayerController* PlayerController = GetOwnerController();
	return ModeState.bAssemblyModeActive
		&& !AreaCopy.IsPlacementActive()
		&& IsValid(PlayerController)
		&& !PlayerController->IsPointerOverBlockingUI();
}

bool USRAssemblyComponent::IsAreaSelectionDragActive() const
{
	return AreaSelection.IsSelectionDragActive();
}

bool USRAssemblyComponent::IsAreaDeletionDragActive() const
{
	return AreaSelection.IsDeletionDragActive();
}

bool USRAssemblyComponent::BeginAreaDeletionDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!ShouldHandleAreaDeletionDrag())
	{
		return false;
	}

	EndStructurePlacementDrag(false);
	ClearAreaSelection();
	ClearAreaDeletion();
	ConveyorPreview.ClearBulkDeletionPreview();

	const ASRPlayerController* PlayerController = GetOwnerController();
	StarRovers::Assembly::FSRAssemblySurfaceCursorTarget CursorTarget;
	if (!StarRovers::Assembly::FSRAssemblySurfaceCursorQuery::TryResolveSurfaceCell(PlayerController, CursorTarget))
	{
		return false;
	}

	AreaSelection.BeginDeletionDrag(CursorTarget.SurfaceGrid, CursorTarget.Cell.CellId);
	OutSelectedActor = CursorTarget.FocusedActor;
	return AreaSelection.UpdateDeletionPreview(CursorTarget.SurfaceGrid, CursorTarget.Cell.CellId);
}

bool USRAssemblyComponent::ContinueAreaDeletionDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!AreaSelection.IsDeletionDragActive() || !AreaSelection.HasDeletionStartCell())
	{
		return false;
	}

	const ASRPlayerController* PlayerController = GetOwnerController();
	StarRovers::Assembly::FSRAssemblySurfaceCursorTarget CursorTarget;
	if (!StarRovers::Assembly::FSRAssemblySurfaceCursorQuery::TryResolveSurfaceCell(PlayerController, CursorTarget)
		|| CursorTarget.SurfaceGrid != AreaSelection.GetDeletionSurfaceGrid())
	{
		return false;
	}

	OutSelectedActor = CursorTarget.FocusedActor;
	return AreaSelection.UpdateDeletionPreview(CursorTarget.SurfaceGrid, CursorTarget.Cell.CellId);
}

void USRAssemblyComponent::EndAreaDeletionDrag()
{
	if (!AreaSelection.IsDeletionDragActive())
	{
		return;
	}

	AreaSelection.EndDeletionDrag();
	USRPlanetSurfaceGrid* DeletionSurfaceGrid = AreaSelection.GetDeletionSurfaceGrid();
	const TArray<FSRPlanetSurfaceGridCellId>& DeletionCellIds = AreaSelection.GetDeletionCellIds();
	if (IsValid(DeletionSurfaceGrid) && !DeletionCellIds.IsEmpty())
	{
		DeleteAreaCells(DeletionSurfaceGrid, DeletionCellIds);
	}
	ClearAreaDeletion();
}

void USRAssemblyComponent::ClearAreaDeletion()
{
	AreaSelection.ClearDeletionPreview();
	AreaSelection.ClearDeletion();
}

bool USRAssemblyComponent::DeleteAreaCells(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (!AreaSelection.DeleteCells(SurfaceGrid, CellIds))
	{
		return false;
	}

	ClearPendingConveyorPathStart();
	PlacementQueue.Reset();
	StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
	ClearSelectedStructureInfo();
	SurfaceState.ResetPublishedHoveredCellInfo();
	return true;
}
