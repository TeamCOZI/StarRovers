#include "Assembly/SRAssemblyComponent.h"

#include "Assembly/SRAssemblyConveyorDragPathBuilder.h"
#include "Assembly/SRAssemblyConveyorPlacement.h"
#include "Camera/SRPlayerController.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool USRAssemblyComponent::BuildConveyorPlacementDragPath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRConveyorNetworkComponent* ConveyorNetwork,
	const FSRStructureData& ConveyorData,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	TArray<FSRPlanetSurfaceGridCellId>& OutPathCellIds) const
{
	return StarRovers::Assembly::FSRAssemblyConveyorDragPathBuilder::BuildPath(
		SurfaceGrid,
		ConveyorNetwork,
		PlacementDrag,
		ConveyorData,
		TargetCellId,
		OutPathCellIds);
}

bool USRAssemblyComponent::UpdateConveyorGhostPreview(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCell& TargetCell,
	USRStructureDataAsset* ConveyorDataAsset)
{
	if (!PlacementDrag.bIsConveyorPlacementDragActive
		|| !PlacementDrag.bHasConveyorDragStartCell
		|| !IsValid(PlacementDrag.ConveyorDragStartSurfaceGrid)
		|| PlacementDrag.ConveyorDragStartSurfaceGrid != SurfaceGrid
		|| !IsValid(SurfaceGrid)
		|| !IsValid(ConveyorDataAsset))
	{
		ConveyorPreview.DestroyGhostActor(HoveredSurfaceGrid);
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = nullptr;
	if (!TryGetFocusedConveyorNetwork(FocusedActor, ConveyorNetwork))
	{
		ConveyorPreview.DestroyGhostActor(HoveredSurfaceGrid);
		return false;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	if (ConveyorData.BuildKind != ESRStructureBuildKind::Conveyor)
	{
		ConveyorPreview.DestroyGhostActor(HoveredSurfaceGrid);
		return false;
	}

	SurfaceGrid->SetHoveredCell(TargetCell.CellId);
	PublishHoveredCellInfo(SurfaceGrid, TargetCell);

	const FSRPlanetSurfaceGridCellId& CurrentAnchorCellId = StarRovers::Assembly::FSRAssemblyConveyorDragPathBuilder::ResolveAnchorCellId(
		PlacementDrag.ConveyorDragStartCellId,
		PlacementDrag.ConveyorDragWaypointCellIds);
	if (!StarRovers::Assembly::FSRAssemblyConveyorDragPathBuilder::IsSegmentWithinExtent(CurrentAnchorCellId, TargetCell.CellId))
	{
		ConveyorPreview.ClearInvalidPlacementPreview();
		return true;
	}

	if (ConveyorPreview.IsGhostActorCurrent(SurfaceGrid, ConveyorDataAsset, TargetCell.CellId))
	{
		return true;
	}

	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (!BuildConveyorPlacementDragPath(
		SurfaceGrid,
		ConveyorNetwork,
		ConveyorData,
		TargetCell.CellId,
		PathCellIds))
	{
		if (AActor* SurfaceOwner = SurfaceGrid->GetOwner())
		{
			if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
			{
				StructureInstanceManager->ClearDeletePreviewedStructures();
			}
		}
		ConveyorPreview.DestroyGhostActor(HoveredSurfaceGrid);
		TArray<FSRPlanetSurfaceGridCellId> InvalidPreviewCellIds;
		InvalidPreviewCellIds.Add(TargetCell.CellId);
		ConveyorPreview.SetInvalidPlacementPreview(SurfaceGrid, InvalidPreviewCellIds);
		return true;
	}

	ConveyorPreview.ClearInvalidPlacementPreview();
	if (AActor* SurfaceOwner = SurfaceGrid->GetOwner())
	{
		if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
		{
			TSet<FName> ReplacementPreviewOccupantIds;
			if (FMath::Max(0, ConveyorData.ConveyorLayer) == 0)
			{
				for (const FSRPlanetSurfaceGridCellId& PathCellId : PathCellIds)
				{
					FSRPlanetSurfaceGridCellInfo CellInfo;
					if (SurfaceGrid->GetCellInfoById(PathCellId, CellInfo)
						&& CellInfo.bOccupied
						&& !CellInfo.OccupantId.IsNone()
						&& StructureInstanceManager->CanDestroyStructureForConstruction(CellInfo.OccupantId))
					{
						ReplacementPreviewOccupantIds.Add(CellInfo.OccupantId);
					}
				}
			}
			StructureInstanceManager->SetConstructionReplacementPreviewedStructures(ReplacementPreviewOccupantIds);
		}
	}

	FSRConveyorBeltPath BeltPath;
	BeltPath.CellIds = MoveTemp(PathCellIds);
	BeltPath.Layer = FMath::Max(0, ConveyorData.ConveyorLayer);
	BeltPath.LayerHeight = ConveyorData.ConveyorLayerHeight;
	BeltPath.NetworkId = StarRovers::Assembly::FSRAssemblyConveyorPlacement::MakeNetworkId(FocusedActor, ConveyorData.ConveyorLayer);
	BeltPath.StructureDataAsset = ConveyorDataAsset;

	TArray<FSRConveyorBeltPath> BeltPaths;
	BeltPaths.Add(BeltPath);
	const ESRAssemblyConveyorGhostUpdateResult GhostUpdateResult = ConveyorPreview.UpdateGhostActor(
		SurfaceGrid,
		HoveredSurfaceGrid,
		ConveyorDataAsset,
		ConveyorData,
		BeltPaths,
		ConveyorNetwork->GetConveyorActorSplineComponentTag(),
		ConveyorNetwork->GetConveyorActorSurfaceOffset(),
		TargetCell.CellId);
	return GhostUpdateResult != ESRAssemblyConveyorGhostUpdateResult::Failed;
}

bool USRAssemblyComponent::TryAddConveyorPlacementDragWaypoint()
{
	if (!PlacementDrag.bIsConveyorPlacementDragActive
		|| !PlacementDrag.bHasConveyorDragStartCell
		|| !IsValid(PlacementDrag.ConveyorDragStartSurfaceGrid))
	{
		return false;
	}

	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* ConveyorDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!IsValid(ConveyorDataAsset))
	{
		return false;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	if (ConveyorData.BuildKind != ESRStructureBuildKind::Conveyor)
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	if (!TryResolveStructurePlacementDragTarget(FocusedActor, SurfaceGrid, TargetCell)
		|| SurfaceGrid != PlacementDrag.ConveyorDragStartSurfaceGrid)
	{
		return false;
	}

	AActor* ConveyorActor = nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = nullptr;
	if (!TryGetFocusedConveyorNetwork(ConveyorActor, ConveyorNetwork))
	{
		return false;
	}

	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (!BuildConveyorPlacementDragPath(SurfaceGrid, ConveyorNetwork, ConveyorData, TargetCell.CellId, PathCellIds))
	{
		return false;
	}

	const FSRPlanetSurfaceGridCellId& PreviousAnchorCellId = PlacementDrag.ConveyorDragWaypointCellIds.IsEmpty()
		? PlacementDrag.ConveyorDragStartCellId
		: PlacementDrag.ConveyorDragWaypointCellIds.Last();
	if (!(PreviousAnchorCellId == TargetCell.CellId))
	{
		PlacementDrag.ConveyorDragWaypointCellIds.Add(TargetCell.CellId);
	}

	ConveyorPreview.bHasConveyorGhostTargetCell = false;
	SurfaceGrid->SetSelectedCell(TargetCell.CellId);
	return UpdateConveyorGhostPreview(SurfaceGrid, TargetCell, ConveyorDataAsset);
}

bool USRAssemblyComponent::CommitConveyorPlacementDrag()
{
	if (!PlacementDrag.bIsConveyorPlacementDragActive || !PlacementDrag.bHasConveyorDragStartCell || !IsValid(PlacementDrag.ConveyorDragStartSurfaceGrid))
	{
		return false;
	}

	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* ConveyorDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!IsValid(ConveyorDataAsset) || ConveyorDataAsset->BuildData().BuildKind != ESRStructureBuildKind::Conveyor)
	{
		return false;
	}

	USRPlanetSurfaceGrid* StartSurfaceGrid = PlacementDrag.ConveyorDragStartSurfaceGrid;
	FSRPlanetSurfaceGridCell TargetCell;
	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* CurrentSurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell CurrentTargetCell;
	bool bUseCurrentTargetCell = false;
	if (TryResolveStructurePlacementDragTarget(FocusedActor, CurrentSurfaceGrid, CurrentTargetCell)
		&& CurrentSurfaceGrid == StartSurfaceGrid)
	{
		const FSRPlanetSurfaceGridCellId& CurrentAnchorCellId = StarRovers::Assembly::FSRAssemblyConveyorDragPathBuilder::ResolveAnchorCellId(
			PlacementDrag.ConveyorDragStartCellId,
			PlacementDrag.ConveyorDragWaypointCellIds);
		if (StarRovers::Assembly::FSRAssemblyConveyorDragPathBuilder::IsSegmentWithinExtent(CurrentAnchorCellId, CurrentTargetCell.CellId))
		{
			TargetCell = CurrentTargetCell;
			bUseCurrentTargetCell = true;
		}
	}

	if (!bUseCurrentTargetCell
		&& (!ConveyorPreview.bHasConveyorGhostTargetCell || !StartSurfaceGrid->GetCellById(ConveyorPreview.ConveyorGhostTargetCellId, TargetCell)))
	{
		return false;
	}

	AActor* SurfaceOwner = StartSurfaceGrid->GetOwner();
	USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
		: nullptr;
	if (!IsValid(ConveyorNetwork))
	{
		return false;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (!BuildConveyorPlacementDragPath(StartSurfaceGrid, ConveyorNetwork, ConveyorData, TargetCell.CellId, PathCellIds))
	{
		return false;
	}

	const FName NetworkId = StarRovers::Assembly::FSRAssemblyConveyorPlacement::MakeNetworkId(SurfaceOwner, ConveyorData.ConveyorLayer);
	StarRovers::Assembly::FSRAssemblyConveyorPlacementResult PlacementResult;
	const bool bPlaced = StarRovers::Assembly::FSRAssemblyConveyorPlacement::TryPlacePath(
		StartSurfaceGrid,
		ConveyorNetwork,
		ConveyorDataAsset,
		ConveyorData,
		PathCellIds,
		NetworkId,
		PlacementHistory,
		PlacementResult);
	if (bPlaced)
	{
		PlacementHistory.RecordConveyor(
			*this,
			StartSurfaceGrid,
			ConveyorNetwork,
			PlacementResult.HistoryBeltPath,
			PlacementResult.HistoryPlacedCellIds,
			PlacementResult.HistoryRemovedNaturalStructures);
	}
	StartSurfaceGrid->ClearSelectedCell();
	return bPlaced;
}
