#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRPlayerController.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool USRAssemblyComponent::ShouldHandleAreaSelectionDrag() const
{
	const ASRPlayerController* PlayerController = GetOwnerController();
	if (!bAssemblyModeActive
		|| AreaCopy.IsPlacementActive()
		|| !IsValid(PlayerController)
		|| PlayerController->IsPointerOverBlockingUi())
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

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	FVector HitLocation = FVector::ZeroVector;
	if (!TryGetFocusedSurfaceGrid(FocusedActor, SurfaceGrid)
		|| !TryProjectCursorToSurfaceCell(SurfaceGrid, TargetCell, HitLocation))
	{
		return false;
	}

	AreaSelection.BeginSelectionDrag(SurfaceGrid, TargetCell.CellId);
	OutSelectedActor = FocusedActor;
	return UpdateAreaSelection(SurfaceGrid, TargetCell);
}

bool USRAssemblyComponent::ContinueAreaSelectionDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!AreaSelection.IsSelectionDragActive() || !AreaSelection.HasSelectionStartCell())
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	FVector HitLocation = FVector::ZeroVector;
	if (!TryGetFocusedSurfaceGrid(FocusedActor, SurfaceGrid)
		|| SurfaceGrid != AreaSelection.GetSelectionSurfaceGrid()
		|| !TryProjectCursorToSurfaceCell(SurfaceGrid, TargetCell, HitLocation))
	{
		return false;
	}

	OutSelectedActor = FocusedActor;
	return UpdateAreaSelection(SurfaceGrid, TargetCell);
}

void USRAssemblyComponent::EndAreaSelectionDrag()
{
	AreaSelection.EndSelectionDrag();
}

void USRAssemblyComponent::ClearAreaSelection()
{
	USRPlanetSurfaceGrid* SelectionSurfaceGrid = AreaSelection.GetSelectionSurfaceGrid();

	if (IsValid(SelectionSurfaceGrid))
	{
		SelectionSurfaceGrid->ClearAreaSelectionCells();

		if (AActor* SurfaceOwner = SelectionSurfaceGrid->GetOwner())
		{
			if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
			{
				StructureInstanceManager->ClearGhostedStructures();
			}
		}
	}

	AreaSelection.ClearSelection();
}

bool USRAssemblyComponent::TryDeleteAreaSelection()
{
	USRPlanetSurfaceGrid* SelectionSurfaceGrid = AreaSelection.GetSelectionSurfaceGrid();
	const TArray<FSRPlanetSurfaceGridCellId>& SelectionCellIds = AreaSelection.GetSelectionCellIds();
	if (!bAssemblyModeActive || !IsValid(SelectionSurfaceGrid) || SelectionCellIds.IsEmpty())
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

bool USRAssemblyComponent::BuildAreaSelectionCellIds(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& StartCellId,
	const FSRPlanetSurfaceGridCellId& EndCellId,
	TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const
{
	return AreaSelection.BuildCellIds(SurfaceGrid, StartCellId, EndCellId, OutCellIds);
}

bool USRAssemblyComponent::UpdateAreaSelection(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell)
{
	if (!IsValid(SurfaceGrid) || !AreaSelection.HasSelectionStartCell())
	{
		return false;
	}

	if (AreaSelection.IsLastSelectionTargetCell(TargetCell.CellId))
	{
		return true;
	}

	TArray<FSRPlanetSurfaceGridCellId> NewAreaSelectionCellIds;
	if (!BuildAreaSelectionCellIds(SurfaceGrid, AreaSelection.GetSelectionStartCellId(), TargetCell.CellId, NewAreaSelectionCellIds))
	{
		return false;
	}

	AreaSelection.SetSelectionCells(MoveTemp(NewAreaSelectionCellIds), TargetCell.CellId);
	const TArray<FSRPlanetSurfaceGridCellId>& SelectionCellIds = AreaSelection.GetSelectionCellIds();
	SurfaceGrid->SetAreaSelectionCells(SelectionCellIds);
	ApplyAreaSelectionGhosts(SurfaceGrid, SelectionCellIds);
	return true;
}

void USRAssemblyComponent::ApplyAreaSelectionGhosts(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	TSet<FName> StructureOccupantIds;
	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (SurfaceGrid->GetCellInfoById(CellId, CellInfo) && CellInfo.bOccupied && !CellInfo.OccupantId.IsNone())
		{
			StructureOccupantIds.Add(CellInfo.OccupantId);
		}
	}

	if (AActor* SurfaceOwner = SurfaceGrid->GetOwner())
	{
		if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
		{
			StructureInstanceManager->SetGhostedStructures(StructureOccupantIds);
		}
	}
}

bool USRAssemblyComponent::ShouldHandleAreaDeletionDrag() const
{
	const ASRPlayerController* PlayerController = GetOwnerController();
	return bAssemblyModeActive
		&& !AreaCopy.IsPlacementActive()
		&& IsValid(PlayerController)
		&& !PlayerController->IsPointerOverBlockingUi();
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
	ClearConveyorBulkDeletionPreview();

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	FVector HitLocation = FVector::ZeroVector;
	if (!TryGetFocusedSurfaceGrid(FocusedActor, SurfaceGrid)
		|| !TryProjectCursorToSurfaceCell(SurfaceGrid, TargetCell, HitLocation))
	{
		return false;
	}

	AreaSelection.BeginDeletionDrag(SurfaceGrid, TargetCell.CellId);
	OutSelectedActor = FocusedActor;
	return UpdateAreaDeletion(SurfaceGrid, TargetCell);
}

bool USRAssemblyComponent::ContinueAreaDeletionDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!AreaSelection.IsDeletionDragActive() || !AreaSelection.HasDeletionStartCell())
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	FVector HitLocation = FVector::ZeroVector;
	if (!TryGetFocusedSurfaceGrid(FocusedActor, SurfaceGrid)
		|| SurfaceGrid != AreaSelection.GetDeletionSurfaceGrid()
		|| !TryProjectCursorToSurfaceCell(SurfaceGrid, TargetCell, HitLocation))
	{
		return false;
	}

	OutSelectedActor = FocusedActor;
	return UpdateAreaDeletion(SurfaceGrid, TargetCell);
}

void USRAssemblyComponent::EndAreaDeletionDrag()
{
	if (!AreaSelection.IsDeletionDragActive())
	{
		return;
	}

	AreaSelection.EndDeletionDrag();
	CommitAreaDeletion();
	ClearAreaDeletion();
}

void USRAssemblyComponent::ClearAreaDeletion()
{
	USRPlanetSurfaceGrid* DeletionSurfaceGrid = AreaSelection.GetDeletionSurfaceGrid();

	if (IsValid(DeletionSurfaceGrid))
	{
		DeletionSurfaceGrid->ClearDeletionPreviewCells();

		if (AActor* SurfaceOwner = DeletionSurfaceGrid->GetOwner())
		{
			if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
			{
				StructureInstanceManager->ClearDeletePreviewedStructures();
			}
		}
	}

	AreaSelection.ClearDeletion();
}

bool USRAssemblyComponent::UpdateAreaDeletion(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell)
{
	if (!IsValid(SurfaceGrid) || !AreaSelection.HasDeletionStartCell())
	{
		return false;
	}

	if (AreaSelection.IsLastDeletionTargetCell(TargetCell.CellId))
	{
		return true;
	}

	TArray<FSRPlanetSurfaceGridCellId> NewAreaDeletionCellIds;
	if (!BuildAreaSelectionCellIds(SurfaceGrid, AreaSelection.GetDeletionStartCellId(), TargetCell.CellId, NewAreaDeletionCellIds))
	{
		return false;
	}

	AreaSelection.SetDeletionCells(MoveTemp(NewAreaDeletionCellIds), TargetCell.CellId);
	const TArray<FSRPlanetSurfaceGridCellId>& DeletionCellIds = AreaSelection.GetDeletionCellIds();
	SurfaceGrid->SetDeletionPreviewCells(DeletionCellIds);
	ApplyAreaDeletionPreview(SurfaceGrid, DeletionCellIds);
	return true;
}

void USRAssemblyComponent::ApplyAreaDeletionPreview(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	TSet<FName> StructureOccupantIds;
	CollectAreaDeletionTargetOccupantIds(SurfaceGrid, CellIds, StructureOccupantIds);
	if (AActor* SurfaceOwner = SurfaceGrid->GetOwner())
	{
		if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
		{
			StructureInstanceManager->SetDeletePreviewedStructures(StructureOccupantIds);
		}
	}
}

bool USRAssemblyComponent::CommitAreaDeletion()
{
	USRPlanetSurfaceGrid* DeletionSurfaceGrid = AreaSelection.GetDeletionSurfaceGrid();
	const TArray<FSRPlanetSurfaceGridCellId>& DeletionCellIds = AreaSelection.GetDeletionCellIds();
	if (!IsValid(DeletionSurfaceGrid) || DeletionCellIds.IsEmpty())
	{
		return false;
	}

	return DeleteAreaCells(DeletionSurfaceGrid, DeletionCellIds);
}

bool USRAssemblyComponent::DeleteAreaCells(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (!IsValid(SurfaceGrid) || CellIds.IsEmpty())
	{
		return false;
	}

	bool bDeletedAny = false;
	AActor* SurfaceOwner = SurfaceGrid->GetOwner();

	if (USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
		: nullptr)
	{
		bDeletedAny |= ConveyorNetwork->TryRemoveConveyorsAtCells(SurfaceGrid, CellIds);
	}

	TSet<FName> StructureOccupantIds;
	CollectAreaDeletionTargetOccupantIds(SurfaceGrid, CellIds, StructureOccupantIds);
	USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	if (!StructureOccupantIds.IsEmpty()
		&& StructureInstanceManager
		&& StructureInstanceManager->RemoveNonResourceStructuresByOccupantIds(SurfaceGrid, StructureOccupantIds))
	{
		bDeletedAny = true;
	}

	if (!bDeletedAny)
	{
		return false;
	}

	ClearPendingConveyorPathStart();
	PlacementQueue.Reset();
	DestroyStructureGhostPreview();
	ClearSelectedStructureInfo();
	bHasLastPublishedHoveredCellInfo = false;
	LastPublishedHoveredSurfaceGrid = nullptr;
	LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
	return true;
}

void USRAssemblyComponent::CollectAreaDeletionTargetOccupantIds(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	TSet<FName>& OutOccupantIds) const
{
	OutOccupantIds.Reset();
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	if (!StructureInstanceManager)
	{
		return;
	}

	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo) || !CellInfo.bOccupied || CellInfo.OccupantId.IsNone())
		{
			continue;
		}

		FSRPlacedStructureInstance PlacedStructure;
		if (!StructureInstanceManager->GetPlacedStructure(CellInfo.OccupantId, PlacedStructure)
			|| !IsValid(PlacedStructure.StructureDataAsset.Get())
			|| PlacedStructure.StructureDataAsset->BuildData().bIsResourceDeposit)
		{
			continue;
		}

		OutOccupantIds.Add(CellInfo.OccupantId);
	}
}
