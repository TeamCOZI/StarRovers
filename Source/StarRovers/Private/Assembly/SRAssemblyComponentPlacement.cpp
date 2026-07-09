#include "Assembly/SRAssemblyComponent.h"

#include "Assembly/SRAssemblyConveyorPlacement.h"
#include "Assembly/SRAssemblySurfaceCursorQuery.h"
#include "Assembly/SRAssemblyStructureDragPathBuilder.h"
#include "Assembly/SRAssemblyStructurePlacement.h"
#include "Camera/SRPlayerController.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

void USRAssemblyComponent::ProcessQueuedStructurePlacements()
{
	if (PlacementQueue.IsEmpty())
	{
		return;
	}

	TArray<FSRQueuedStructurePlacement> QueuedPlacements;
	PlacementQueue.PopNextFrame(QueuedPlacements);
	TSet<USRPlanetSurfaceGrid*> BatchedSurfaceGrids;
	BatchedSurfaceGrids.Reserve(QueuedPlacements.Num());
	bool bPlacedAnyStructure = false;

	for (const FSRQueuedStructurePlacement& QueuedPlacement : QueuedPlacements)
	{
		USRPlanetSurfaceGrid* SurfaceGrid = QueuedPlacement.SurfaceGrid.Get();
		if (!IsValid(SurfaceGrid))
		{
			continue;
		}

		if (!BatchedSurfaceGrids.Contains(SurfaceGrid))
		{
			SurfaceGrid->BeginInteractionHighlightBatch();
			BatchedSurfaceGrids.Add(SurfaceGrid);
		}

		FSRPlanetSurfaceGridCell TargetCell;
		if (SurfaceGrid->GetCellById(QueuedPlacement.CellId, TargetCell))
		{
			bPlacedAnyStructure |= TryPlaceSelectedStructure(SurfaceGrid, TargetCell, false, QueuedPlacement.PlacementRotationSteps);
		}
	}

	for (USRPlanetSurfaceGrid* SurfaceGrid : BatchedSurfaceGrids)
	{
		if (IsValid(SurfaceGrid))
		{
			SurfaceGrid->EndInteractionHighlightBatch();
		}
	}

	if (bPlacedAnyStructure)
	{
		StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
		if (IsValid(HoveredSurfaceGrid))
		{
			FSRPlanetSurfaceGridCell HoveredCell;
			if (HoveredSurfaceGrid->GetHoveredCell(HoveredCell))
			{
				SurfaceState.ResetPublishedHoveredCellInfo();
				PublishHoveredCellInfo(HoveredSurfaceGrid, HoveredCell);
			}
		}
	}
}

bool USRAssemblyComponent::TryResolveStructurePlacementDragTarget(
	AActor*& OutFocusedActor,
	USRPlanetSurfaceGrid*& OutSurfaceGrid,
	FSRPlanetSurfaceGridCell& OutTargetCell) const
{
	OutFocusedActor = nullptr;
	OutSurfaceGrid = nullptr;
	OutTargetCell = FSRPlanetSurfaceGridCell();

	const ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController
		|| PlayerController->IsPointerOverBlockingUI()
		|| !ModeState.bAssemblyModeActive
		|| !IsValid(PlayerController->GetSelectedStructureDataAsset()))
	{
		return false;
	}

	StarRovers::Assembly::FSRAssemblySurfaceCursorTarget CursorTarget;
	if (!StarRovers::Assembly::FSRAssemblySurfaceCursorQuery::TryResolveSurfaceCell(PlayerController, CursorTarget))
	{
		return false;
	}

	OutFocusedActor = CursorTarget.FocusedActor;
	OutSurfaceGrid = CursorTarget.SurfaceGrid;
	OutTargetCell = CursorTarget.Cell;
	return true;
}

bool USRAssemblyComponent::TryGetFocusedConveyorNetwork(AActor*& OutFocusedActor, USRConveyorNetworkComponent*& OutConveyorNetwork) const
{
	OutFocusedActor = nullptr;
	OutConveyorNetwork = nullptr;

	USRPlanetSurfaceGrid* UnusedSurfaceGrid = nullptr;
	if (!StarRovers::Assembly::FSRAssemblySurfaceCursorQuery::TryGetFocusedSurfaceGrid(GetOwnerController(), OutFocusedActor, UnusedSurfaceGrid)
		|| !IsValid(OutFocusedActor))
	{
		return false;
	}

	OutConveyorNetwork = OutFocusedActor->FindComponentByClass<USRConveyorNetworkComponent>();
	return IsValid(OutConveyorNetwork);
}

void USRAssemblyComponent::EnqueueStructurePlacement(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& CellId)
{
	PlacementQueue.Enqueue(SurfaceGrid, CellId, GetStructurePlacementRotationSteps());
}

bool USRAssemblyComponent::TryPlaceStructureDragPath(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell)
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (IsValid(SelectedStructureDataAsset) && SelectedStructureDataAsset->BuildData().BuildKind == ESRStructureBuildKind::Conveyor)
	{
		return TryPlaceConveyorDragPath(SurfaceGrid, TargetCell, SelectedStructureDataAsset);
	}

	if (PlacementDrag.IsLastStructurePlacementDragCell(SurfaceGrid, TargetCell.CellId))
	{
		return true;
	}

	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (!StarRovers::Assembly::FSRAssemblyStructureDragPathBuilder::BuildQueuedCellIds(
		PlacementDrag,
		SurfaceGrid,
		TargetCell.CellId,
		PathCellIds))
	{
		return false;
	}

	for (const FSRPlanetSurfaceGridCellId& PathCellId : PathCellIds)
	{
		EnqueueStructurePlacement(SurfaceGrid, PathCellId);
	}

	SurfaceGrid->SetHoveredCell(TargetCell.CellId);
	PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	PlacementDrag.SetLastStructurePlacementDragCell(SurfaceGrid, TargetCell.CellId);
	return !PathCellIds.IsEmpty();
}

bool USRAssemblyComponent::TryPlaceConveyorDragPath(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset)
{
	if (!IsValid(SurfaceGrid) || !IsValid(ConveyorDataAsset))
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = nullptr;
	if (!TryGetFocusedConveyorNetwork(FocusedActor, ConveyorNetwork))
	{
		return false;
	}

	if (PlacementDrag.IsLastStructurePlacementDragCell(SurfaceGrid, TargetCell.CellId))
	{
		return true;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (PlacementDrag.HasLastStructurePlacementDragCell(SurfaceGrid))
	{
		if (!ConveyorNetwork->FindConveyorPath(SurfaceGrid, PlacementDrag.LastStructurePlacementDragCellId, TargetCell.CellId, ConveyorData.ConveyorLayer, PathCellIds))
		{
			return false;
		}
	}
	else
	{
		PathCellIds.Add(TargetCell.CellId);
	}

	const FName NetworkId = StarRovers::Assembly::FSRAssemblyConveyorPlacement::MakeNetworkId(FocusedActor, ConveyorData.ConveyorLayer);
	StarRovers::Assembly::FSRAssemblyConveyorPlacementResult PlacementResult;
	if (!StarRovers::Assembly::FSRAssemblyConveyorPlacement::TryPlacePath(
		SurfaceGrid,
		ConveyorNetwork,
		ConveyorDataAsset,
		ConveyorData,
		PathCellIds,
		NetworkId,
		PlacementHistory,
		PlacementResult))
	{
		return false;
	}

	PlacementHistory.RecordConveyor(
		*this,
		SurfaceGrid,
		ConveyorNetwork,
		PlacementResult.HistoryBeltPath,
		PlacementResult.HistoryPlacedCellIds,
		PlacementResult.HistoryRemovedNaturalStructures);
	SurfaceGrid->SetHoveredCell(TargetCell.CellId);
	PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	PlacementDrag.SetLastStructurePlacementDragCell(SurfaceGrid, TargetCell.CellId);
	return true;
}

bool USRAssemblyComponent::CommitStructurePlacementDrag()
{
	USRPlanetSurfaceGrid* SurfaceGrid = PlacementDrag.StructurePlacementDragSurfaceGrid.Get();
	if (!PlacementDrag.bIsStructurePlacementDragActive || !IsValid(SurfaceGrid) || PlacementDrag.StructurePlacementDragCellIds.IsEmpty())
	{
		return false;
	}

	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!IsValid(SelectedStructureDataAsset) || SelectedStructureDataAsset->BuildData().BuildKind != ESRStructureBuildKind::Structure)
	{
		return false;
	}

	bool bPlacedAnyStructure = false;
	TArray<FSRAssemblyPlacementHistoryEntry> HistoryEntries;
	HistoryEntries.Reserve(PlacementDrag.StructurePlacementDragCellIds.Num());

	SurfaceGrid->BeginInteractionHighlightBatch();
	for (const FSRPlanetSurfaceGridCellId& CellId : PlacementDrag.StructurePlacementDragCellIds)
	{
		FSRPlanetSurfaceGridCell TargetCell;
		if (!SurfaceGrid->GetCellById(CellId, TargetCell))
		{
			continue;
		}

		bPlacedAnyStructure |= TryPlaceSelectedStructure(
			SurfaceGrid,
			TargetCell,
			false,
			PlacementDrag.StructurePlacementDragRotationSteps,
			&HistoryEntries);
	}
	SurfaceGrid->EndInteractionHighlightBatch();

	if (!bPlacedAnyStructure)
	{
		return false;
	}

	PlacementHistory.RecordBatch(*this, SurfaceGrid, HistoryEntries);
	StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
	SurfaceState.ResetPublishedHoveredCellInfo();

	FSRPlanetSurfaceGridCell HoveredCell;
	if (SurfaceGrid->GetHoveredCell(HoveredCell))
	{
		PublishHoveredCellInfo(SurfaceGrid, HoveredCell);
	}
	return true;
}

bool USRAssemblyComponent::TryPlaceSelectedStructure(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCell& TargetCell,
	bool bRefreshPreviewAndUI,
	int32 PlacementRotationStepsOverride,
	TArray<FSRAssemblyPlacementHistoryEntry>* OutHistoryEntries)
{
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!IsValid(PlayerController) || !IsValid(SurfaceGrid) || !IsValid(SelectedStructureDataAsset))
	{
		return false;
	}

	const FSRStructureData StructureData = SelectedStructureDataAsset->BuildData();
	if (StructureData.BuildKind == ESRStructureBuildKind::Conveyor)
	{
		return TryPlaceSelectedConveyor(SurfaceGrid, TargetCell, SelectedStructureDataAsset, bRefreshPreviewAndUI);
	}
	const int32 PlacementRotationSteps = PlacementRotationStepsOverride == INDEX_NONE
		? GetStructurePlacementRotationSteps()
		: StarRovers::Structure::NormalizePlacementRotationSteps(PlacementRotationStepsOverride);

	StarRovers::Assembly::FSRAssemblyStructurePlacementResult PlacementResult;
	if (!StarRovers::Assembly::FSRAssemblyStructurePlacement::TryPlace(
		SurfaceGrid,
		SelectedStructureDataAsset,
		StructureData,
		TargetCell.CellId,
		PlacementRotationSteps,
		PlacementResult))
	{
		if (bRefreshPreviewAndUI && PlacementResult.bShouldDestroyPreviewOnFailure)
		{
			StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
		}
		return false;
	}

	if (PlacementResult.bPlacedWithStructureInstanceManager)
	{
		USRStructureInstanceManagerComponent* StructureInstanceManager = PlacementResult.StructureInstanceManager.Get();
		if (IsValid(StructureInstanceManager))
		{
			if (OutHistoryEntries)
			{
				OutHistoryEntries->Add(StarRovers::Assembly::FSRAssemblyStructurePlacement::BuildHistoryEntry(
					SurfaceGrid,
					StructureInstanceManager,
					SelectedStructureDataAsset,
					TargetCell.CellId,
					PlacementRotationSteps,
					PlacementResult));
			}
			else
			{
				PlacementHistory.RecordStructure(
					*this,
					SurfaceGrid,
					StructureInstanceManager,
					SelectedStructureDataAsset,
					TargetCell.CellId,
					PlacementRotationSteps,
					PlacementResult.OccupantId,
					PlacementResult.RemovedNaturalStructures);
			}
		}
	}

	if (bRefreshPreviewAndUI)
	{
		StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);

		SurfaceState.ResetPublishedHoveredCellInfo();
		PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	}
	return true;
}

bool USRAssemblyComponent::TryPlaceSelectedConveyor(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset, bool bRefreshPreviewAndUI)
{
	if (!IsValid(SurfaceGrid) || !IsValid(ConveyorDataAsset))
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = nullptr;
	if (!TryGetFocusedConveyorNetwork(FocusedActor, ConveyorNetwork))
	{
		return false;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	const TArray<FSRPlanetSurfaceGridCellId> PathCellIds = { TargetCell.CellId };
	const FName NetworkId = StarRovers::Assembly::FSRAssemblyConveyorPlacement::MakeNetworkId(FocusedActor, ConveyorData.ConveyorLayer);
	StarRovers::Assembly::FSRAssemblyConveyorPlacementResult PlacementResult;
	if (!StarRovers::Assembly::FSRAssemblyConveyorPlacement::TryPlacePath(
		SurfaceGrid,
		ConveyorNetwork,
		ConveyorDataAsset,
		ConveyorData,
		PathCellIds,
		NetworkId,
		PlacementHistory,
		PlacementResult))
	{
		return false;
	}

	PlacementHistory.RecordConveyor(
		*this,
		SurfaceGrid,
		ConveyorNetwork,
		PlacementResult.HistoryBeltPath,
		PlacementResult.HistoryPlacedCellIds,
		PlacementResult.HistoryRemovedNaturalStructures);
	if (bRefreshPreviewAndUI)
	{
		StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
		SurfaceState.ResetPublishedHoveredCellInfo();
		PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	}
	return true;
}

bool USRAssemblyComponent::TryPlaceSelectedConveyorPath(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& StartCellId, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset, bool bRefreshPreviewAndUI)
{
	if (!IsValid(SurfaceGrid) || !IsValid(ConveyorDataAsset))
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = nullptr;
	if (!TryGetFocusedConveyorNetwork(FocusedActor, ConveyorNetwork))
	{
		return false;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (!ConveyorNetwork->FindConveyorPath(SurfaceGrid, StartCellId, TargetCell.CellId, ConveyorData.ConveyorLayer, PathCellIds))
	{
		return false;
	}

	const FName NetworkId = StarRovers::Assembly::FSRAssemblyConveyorPlacement::MakeNetworkId(FocusedActor, ConveyorData.ConveyorLayer);
	StarRovers::Assembly::FSRAssemblyConveyorPlacementResult PlacementResult;
	if (!StarRovers::Assembly::FSRAssemblyConveyorPlacement::TryPlacePath(
		SurfaceGrid,
		ConveyorNetwork,
		ConveyorDataAsset,
		ConveyorData,
		PathCellIds,
		NetworkId,
		PlacementHistory,
		PlacementResult))
	{
		return false;
	}

	PlacementHistory.RecordConveyor(
		*this,
		SurfaceGrid,
		ConveyorNetwork,
		PlacementResult.HistoryBeltPath,
		PlacementResult.HistoryPlacedCellIds,
		PlacementResult.HistoryRemovedNaturalStructures);
	if (bRefreshPreviewAndUI)
	{
		StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
		SurfaceState.ResetPublishedHoveredCellInfo();
		PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	}
	return true;
}

void USRAssemblyComponent::ClearPendingConveyorPathStart()
{
	if (IsValid(PlacementDrag.PendingConveyorStartSurfaceGrid))
	{
		PlacementDrag.PendingConveyorStartSurfaceGrid->ClearSelectedCell();
	}

	PlacementDrag.ResetPendingConveyorStart();
}
