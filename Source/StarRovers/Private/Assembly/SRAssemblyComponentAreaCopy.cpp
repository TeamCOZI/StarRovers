#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRPlayerController.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool USRAssemblyComponent::IsAreaCopyPlacementActive() const
{
	return AreaCopy.IsPlacementActive();
}

bool USRAssemblyComponent::TryBeginAreaSelectionCopyPlacement()
{
	USRPlanetSurfaceGrid* SelectionSurfaceGrid = AreaSelection.GetSelectionSurfaceGrid();
	const TArray<FSRPlanetSurfaceGridCellId>& SelectionCellIds = AreaSelection.GetSelectionCellIds();
	if (!ModeState.bAssemblyModeActive || !IsValid(SelectionSurfaceGrid) || SelectionCellIds.IsEmpty())
	{
		return false;
	}

	FSRPlanetSurfaceGridCellId SelectionCenterCellId;
	if (!AreaSelection.ResolveSelectionCenterCellId(SelectionCenterCellId))
	{
		return false;
	}

	AActor* SurfaceOwner = SelectionSurfaceGrid->GetOwner();
	USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
		: nullptr;

	TArray<FSRAssemblyAreaCopiedStructure> NewCopiedStructures;
	TArray<FSRAssemblyAreaCopiedConveyorPath> NewCopiedConveyorPaths;
	if (!StarRovers::Assembly::FSRAssemblyAreaCopy::BuildPlacementPayloadFromSelection(
		SelectionCellIds,
		SelectionCenterCellId,
		StructureInstanceManager,
		ConveyorNetwork,
		NewCopiedStructures,
		NewCopiedConveyorPaths))
	{
		return false;
	}

	EndStructurePlacementDrag(false);
	ClearAreaDeletion();
	ClearPendingConveyorPathStart();
	PlacementQueue.Reset();
	FSRAssemblyPreviewResetOptions PreviewResetOptions;
	PreviewResetOptions.bDestroyConveyorDeletionGhostActor = true;
	FSRAssemblyPreviewReset::Apply(StructurePreview, ConveyorPreview, HoveredSurfaceGrid, PreviewResetOptions);
	AreaCopy.DestroyPreviewActors(HoveredSurfaceGrid);

	AreaCopy.BeginPlacement(MoveTemp(NewCopiedStructures), MoveTemp(NewCopiedConveyorPaths));

	ClearAreaSelection();
	RebuildAreaCopyPreviewActors();
	UpdateAreaCopyPlacementPreview();
	return true;
}

bool USRAssemblyComponent::MirrorAreaCopyPlacement(bool bMirrorLeftRight)
{
	if (!AreaCopy.MirrorPlacement(bMirrorLeftRight))
	{
		return false;
	}

	UpdateAreaCopyPlacementPreview();
	return true;
}

bool USRAssemblyComponent::RotateAreaCopyPlacement(int32 StepDelta)
{
	if (!AreaCopy.RotatePlacement(StepDelta))
	{
		return false;
	}

	UpdateAreaCopyPlacementPreview();
	return true;
}

void USRAssemblyComponent::CancelAreaCopyPlacement()
{
	if (!AreaCopy.IsPlacementActive() && !AreaCopy.HasPayload())
	{
		return;
	}

	AreaCopy.DestroyPreviewActors(HoveredSurfaceGrid);
	AreaCopy.Cancel();
}

void USRAssemblyComponent::RebuildAreaCopyPreviewActors()
{
	UWorld* World = GetWorld();
	ASRPlayerController* PlayerController = GetOwnerController();
	if (!IsValid(World) || !IsValid(PlayerController))
	{
		AreaCopy.DestroyPreviewActors(HoveredSurfaceGrid);
		return;
	}

	AActor* SurfaceOwner = IsValid(HoveredSurfaceGrid) ? HoveredSurfaceGrid->GetOwner() : nullptr;
	USRPlanetSurfaceGrid* SelectionSurfaceGrid = AreaSelection.GetSelectionSurfaceGrid();
	if (!IsValid(SurfaceOwner) && IsValid(SelectionSurfaceGrid))
	{
		SurfaceOwner = SelectionSurfaceGrid->GetOwner();
	}

	AreaCopy.RebuildPreviewActors(World, SurfaceOwner, PlayerController, HoveredSurfaceGrid);
}

void USRAssemblyComponent::UpdateAreaCopyPlacementPreview()
{
	USRPlanetSurfaceGrid* SurfaceGrid = HoveredSurfaceGrid.Get();
	FSRPlanetSurfaceGridCell HoveredCell;
	if (!IsValid(SurfaceGrid) || !SurfaceGrid->GetHoveredCell(HoveredCell))
	{
		return;
	}

	AreaCopy.UpdatePlacementPreview(SurfaceGrid, HoveredCell.CellId);
}

bool USRAssemblyComponent::TryCommitAreaCopyPlacement(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!AreaCopy.IsPlacementActive() || !AreaCopy.HasPayload())
	{
		return false;
	}

	USRPlanetSurfaceGrid* SurfaceGrid = HoveredSurfaceGrid.Get();
	FSRPlanetSurfaceGridCell HoveredCell;
	if (!IsValid(SurfaceGrid) || !SurfaceGrid->GetHoveredCell(HoveredCell))
	{
		return true;
	}

	FSRAssemblyAreaCopyCommitResult CommitResult;
	if (!AreaCopy.TryCommitPlacement(SurfaceGrid, HoveredCell.CellId, PlacementHistory, CommitResult))
	{
		return false;
	}

	if (!CommitResult.bPlacedAny)
	{
		return true;
	}

	PlacementHistory.RecordBatch(*this, SurfaceGrid, CommitResult.HistoryEntries);
	OutSelectedActor = CommitResult.SurfaceOwner.Get();
	AreaCopy.ClearPreviewHoverCache();
	UpdateAreaCopyPlacementPreview();
	SurfaceState.ResetPublishedHoveredCellInfo();
	PublishHoveredCellInfo(SurfaceGrid, HoveredCell);
	return true;
}
