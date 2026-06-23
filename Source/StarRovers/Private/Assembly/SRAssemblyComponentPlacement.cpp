#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRPlayerController.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Structure/SRStructurePlacementLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	void AppendGridLineCellIds(
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const FSRPlanetSurfaceGridCellId& EndCellId,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
	{
		if (StartCellId.Face != EndCellId.Face)
		{
			OutCellIds.Add(EndCellId);
			return;
		}

		const int32 DeltaX = FMath::Abs(EndCellId.CellX - StartCellId.CellX);
		const int32 DeltaY = FMath::Abs(EndCellId.CellY - StartCellId.CellY);
		const int32 StepX = StartCellId.CellX < EndCellId.CellX ? 1 : -1;
		const int32 StepY = StartCellId.CellY < EndCellId.CellY ? 1 : -1;

		int32 CurrentX = StartCellId.CellX;
		int32 CurrentY = StartCellId.CellY;
		int32 Error = DeltaX - DeltaY;

		while (true)
		{
			FSRPlanetSurfaceGridCellId CellId;
			CellId.Face = StartCellId.Face;
			CellId.CellX = CurrentX;
			CellId.CellY = CurrentY;
			OutCellIds.Add(CellId);

			if (CurrentX == EndCellId.CellX && CurrentY == EndCellId.CellY)
			{
				break;
			}

			const int32 Error2 = Error * 2;
			if (Error2 > -DeltaY)
			{
				Error -= DeltaY;
				CurrentX += StepX;
			}
			if (Error2 < DeltaX)
			{
				Error += DeltaX;
				CurrentY += StepY;
			}
		}
	}
}

void USRAssemblyComponent::ProcessQueuedStructurePlacements()
{
	if (PendingStructurePlacementQueue.IsEmpty())
	{
		return;
	}

	const int32 PlacementBudget = FMath::Max(1, MaxStructurePlacementsPerFrame);
	TSet<USRPlanetSurfaceGrid*> BatchedSurfaceGrids;
	BatchedSurfaceGrids.Reserve(PlacementBudget);
	bool bPlacedAnyStructure = false;

	const int32 PlacementCount = FMath::Min(PlacementBudget, PendingStructurePlacementQueue.Num());
	for (int32 PlacementIndex = 0; PlacementIndex < PlacementCount; ++PlacementIndex)
	{
		FSRQueuedStructurePlacement QueuedPlacement = PendingStructurePlacementQueue[0];
		PendingStructurePlacementQueue.RemoveAt(0, 1, EAllowShrinking::No);

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
		DestroyStructureGhostPreview();
		if (IsValid(HoveredSurfaceGrid))
		{
			FSRPlanetSurfaceGridCell HoveredCell;
			if (HoveredSurfaceGrid->GetHoveredCell(HoveredCell))
			{
				bHasLastPublishedHoveredCellInfo = false;
				LastPublishedHoveredSurfaceGrid = nullptr;
				LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
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
		|| PlayerController->IsPointerOverBlockingUi()
		|| !bAssemblyModeActive
		|| !IsValid(PlayerController->GetSelectedStructureDataAsset()))
	{
		return false;
	}

	FVector HoverHitLocation = FVector::ZeroVector;
	return TryGetFocusedSurfaceGrid(OutFocusedActor, OutSurfaceGrid)
		&& TryProjectCursorToSurfaceCell(OutSurfaceGrid, OutTargetCell, HoverHitLocation);
}

bool USRAssemblyComponent::TryGetFocusedConveyorNetwork(AActor*& OutFocusedActor, USRConveyorNetworkComponent*& OutConveyorNetwork) const
{
	OutFocusedActor = nullptr;
	OutConveyorNetwork = nullptr;

	USRPlanetSurfaceGrid* UnusedSurfaceGrid = nullptr;
	if (!TryGetFocusedSurfaceGrid(OutFocusedActor, UnusedSurfaceGrid) || !IsValid(OutFocusedActor))
	{
		return false;
	}

	OutConveyorNetwork = OutFocusedActor->FindComponentByClass<USRConveyorNetworkComponent>();
	return IsValid(OutConveyorNetwork);
}

void USRAssemblyComponent::EnqueueStructurePlacement(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& CellId)
{
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	for (const FSRQueuedStructurePlacement& PendingPlacement : PendingStructurePlacementQueue)
	{
		if (PendingPlacement.SurfaceGrid.Get() == SurfaceGrid && PendingPlacement.CellId == CellId)
		{
			return;
		}
	}

	const int32 MaxQueueSize = FMath::Max(1, MaxQueuedStructurePlacements);
	if (PendingStructurePlacementQueue.Num() >= MaxQueueSize)
	{
		PendingStructurePlacementQueue.RemoveAt(0, PendingStructurePlacementQueue.Num() - MaxQueueSize + 1, EAllowShrinking::No);
	}

	FSRQueuedStructurePlacement QueuedPlacement;
	QueuedPlacement.SurfaceGrid = SurfaceGrid;
	QueuedPlacement.CellId = CellId;
	QueuedPlacement.PlacementRotationSteps = GetStructurePlacementRotationSteps();
	PendingStructurePlacementQueue.Add(QueuedPlacement);
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

	if (LastStructurePlacementDragSurfaceGrid == SurfaceGrid
		&& bHasLastStructurePlacementDragCellId
		&& LastStructurePlacementDragCellId == TargetCell.CellId)
	{
		return true;
	}

	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (LastStructurePlacementDragSurfaceGrid == SurfaceGrid && bHasLastStructurePlacementDragCellId)
	{
		AppendGridLineCellIds(LastStructurePlacementDragCellId, TargetCell.CellId, PathCellIds);
	}
	else
	{
		PathCellIds.Add(TargetCell.CellId);
	}

	for (const FSRPlanetSurfaceGridCellId& PathCellId : PathCellIds)
	{
		if (LastStructurePlacementDragSurfaceGrid == SurfaceGrid
			&& bHasLastStructurePlacementDragCellId
			&& LastStructurePlacementDragCellId == PathCellId)
		{
			continue;
		}

		EnqueueStructurePlacement(SurfaceGrid, PathCellId);
	}

	SurfaceGrid->SetHoveredCell(TargetCell.CellId);
	PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	LastStructurePlacementDragSurfaceGrid = SurfaceGrid;
	LastStructurePlacementDragCellId = TargetCell.CellId;
	bHasLastStructurePlacementDragCellId = true;
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

	if (LastStructurePlacementDragSurfaceGrid == SurfaceGrid
		&& bHasLastStructurePlacementDragCellId
		&& LastStructurePlacementDragCellId == TargetCell.CellId)
	{
		return true;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (LastStructurePlacementDragSurfaceGrid == SurfaceGrid && bHasLastStructurePlacementDragCellId)
	{
		if (!ConveyorNetwork->FindConveyorPath(SurfaceGrid, LastStructurePlacementDragCellId, TargetCell.CellId, ConveyorData.ConveyorLayer, PathCellIds))
		{
			return false;
		}
	}
	else
	{
		PathCellIds.Add(TargetCell.CellId);
	}

	const FName NetworkId = FName(*FString::Printf(TEXT("Conveyor_%s_%d"), *GetNameSafe(FocusedActor), static_cast<int32>(ConveyorData.ConveyorLayer)));
	if (!ConveyorNetwork->TryPlaceConveyorPath(
		SurfaceGrid,
		PathCellIds,
		ConveyorData.ConveyorLayer,
		ConveyorData.ConveyorLayerHeight,
		ConveyorDataAsset,
		NetworkId))
	{
		return false;
	}

	SurfaceGrid->SetHoveredCell(TargetCell.CellId);
	PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	LastStructurePlacementDragSurfaceGrid = SurfaceGrid;
	LastStructurePlacementDragCellId = TargetCell.CellId;
	bHasLastStructurePlacementDragCellId = true;
	return true;
}

bool USRAssemblyComponent::TryPlaceSelectedStructure(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, bool bRefreshPreviewAndUI, int32 PlacementRotationStepsOverride)
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

	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
	if (!SurfaceGrid->GetFootprintCellIds(
		TargetCell.CellId,
		StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, PlacementRotationSteps),
		StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, PlacementRotationSteps),
		FootprintCellIds)
		|| !SurfaceGrid->CanOccupyCells(FootprintCellIds))
	{
		if (bRefreshPreviewAndUI)
		{
			DestroyStructureGhostPreview();
		}
		return false;
	}

	if (AActor* SurfaceOwner = SurfaceGrid->GetOwner())
	{
		if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
		{
			FName OccupantId = NAME_None;
			if (StructureInstanceManager->TryPlaceStructureOnSurfaceGrid(
				SurfaceGrid,
				TargetCell.CellId,
				SelectedStructureDataAsset,
				OccupantId,
				false,
				false,
				PlacementRotationSteps))
			{
				if (bRefreshPreviewAndUI)
				{
					DestroyStructureGhostPreview();

					bHasLastPublishedHoveredCellInfo = false;
					LastPublishedHoveredSurfaceGrid = nullptr;
					LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
					PublishHoveredCellInfo(SurfaceGrid, TargetCell);
				}
				return true;
			}
		}
	}

	AActor* PlacedStructureActor = nullptr;
	if (!USRStructurePlacementLibrary::TryPlaceStructureOnSurfaceGrid(
		SurfaceGrid,
		TargetCell.CellId,
		SelectedStructureDataAsset,
		PlacedStructureActor,
		false,
		PlacementRotationSteps))
	{
		return false;
	}

	if (bRefreshPreviewAndUI)
	{
		DestroyStructureGhostPreview();

		bHasLastPublishedHoveredCellInfo = false;
		LastPublishedHoveredSurfaceGrid = nullptr;
		LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
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
	const FName NetworkId = FName(*FString::Printf(TEXT("Conveyor_%s_%d"), *GetNameSafe(FocusedActor), static_cast<int32>(ConveyorData.ConveyorLayer)));
	if (!ConveyorNetwork->TryPlaceConveyorPath(
		SurfaceGrid,
		PathCellIds,
		ConveyorData.ConveyorLayer,
		ConveyorData.ConveyorLayerHeight,
		ConveyorDataAsset,
		NetworkId))
	{
		return false;
	}

	if (bRefreshPreviewAndUI)
	{
		DestroyStructureGhostPreview();
		bHasLastPublishedHoveredCellInfo = false;
		LastPublishedHoveredSurfaceGrid = nullptr;
		LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
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

	const FName NetworkId = FName(*FString::Printf(TEXT("Conveyor_%s_%d"), *GetNameSafe(FocusedActor), static_cast<int32>(ConveyorData.ConveyorLayer)));
	if (!ConveyorNetwork->TryPlaceConveyorPath(
		SurfaceGrid,
		PathCellIds,
		ConveyorData.ConveyorLayer,
		ConveyorData.ConveyorLayerHeight,
		ConveyorDataAsset,
		NetworkId))
	{
		return false;
	}

	if (bRefreshPreviewAndUI)
	{
		DestroyStructureGhostPreview();
		bHasLastPublishedHoveredCellInfo = false;
		LastPublishedHoveredSurfaceGrid = nullptr;
		LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
		PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	}
	return true;
}

void USRAssemblyComponent::ClearPendingConveyorPathStart()
{
	if (IsValid(PendingConveyorStartSurfaceGrid))
	{
		PendingConveyorStartSurfaceGrid->ClearSelectedCell();
	}

	PendingConveyorStartSurfaceGrid = nullptr;
	PendingConveyorStartCellId = FSRPlanetSurfaceGridCellId();
	bHasPendingConveyorStartCell = false;
}
