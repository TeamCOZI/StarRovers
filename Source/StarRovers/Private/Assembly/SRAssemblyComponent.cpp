#include "Assembly/SRAssemblyComponent.h"

#include "Assembly/SRAssemblySingleCellDeletion.h"
#include "Assembly/SRAssemblySurfaceCursorQuery.h"
#include "Camera/SRPlayerController.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

USRAssemblyComponent::USRAssemblyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	ModeState = FSRAssemblyModeState();
	SurfaceState = FSRAssemblySurfaceState();
	StructurePreview = FSRAssemblyStructurePreviewState();
	ConveyorPreview = FSRAssemblyConveyorPreviewState();
	PlacementDrag = FSRAssemblyPlacementDragState();
}

void USRAssemblyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateSurfaceHover();
	ProcessQueuedStructurePlacements();
	if (AreaCopy.IsPlacementActive())
	{
		FSRAssemblyPreviewReset::Apply(StructurePreview, ConveyorPreview, HoveredSurfaceGrid);
		UpdateAreaCopyPlacementPreview();
		return;
	}

	const ASRPlayerController* PlayerController = GetOwnerController();
	if (PlayerController
		&& PlayerController->IsConveyorBulkDeleteModifierActive()
		&& !PlayerController->IsAssemblyShiftModifierActive())
	{
		if (!UpdateConveyorBulkDeletionPreview())
		{
			ConveyorPreview.ClearBulkDeletionPreview();
		}
		FSRAssemblyPreviewResetOptions PreviewResetOptions;
		PreviewResetOptions.bClearConveyorBulkDeletionPreview = false;
		FSRAssemblyPreviewReset::Apply(StructurePreview, ConveyorPreview, HoveredSurfaceGrid, PreviewResetOptions);
		return;
	}

	ConveyorPreview.ClearBulkDeletionPreview();
	if (PlacementDrag.bIsStructurePlacementDragActive)
	{
		ConveyorPreview.ClearPortPreview();
		StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
		return;
	}
	UpdateConveyorPlacementPortPreview();
	UpdateStructureGhostPreview();
}

void USRAssemblyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PlacementHistory.Clear();
	ClearAreaSelection();
	ClearAreaDeletion();
	CancelAreaCopyPlacement();
	FSRAssemblyPreviewResetOptions PreviewResetOptions;
	PreviewResetOptions.bDestroyStructurePlacementDragPreviewActors = true;
	PreviewResetOptions.bDestroyConveyorDeletionGhostActor = true;
	FSRAssemblyPreviewReset::Apply(StructurePreview, ConveyorPreview, HoveredSurfaceGrid, PreviewResetOptions);

	Super::EndPlay(EndPlayReason);
}

bool USRAssemblyComponent::IsAssemblyModeActive() const
{
	return ModeState.bAssemblyModeActive;
}

void USRAssemblyComponent::SetAssemblyModeActive(bool bNewAssemblyModeActive)
{
	if (bNewAssemblyModeActive)
	{
		AActor* FocusedActor = nullptr;
		USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
		if (!StarRovers::Assembly::FSRAssemblySurfaceCursorQuery::TryGetFocusedSurfaceGrid(GetOwnerController(), FocusedActor, FocusedSurfaceGrid))
		{
			bNewAssemblyModeActive = false;
		}
	}

	if (ModeState.bAssemblyModeActive == bNewAssemblyModeActive)
	{
		ApplyAssemblyModeToFocusedSurfaceGrid();
		return;
	}

	ModeState.bAssemblyModeActive = bNewAssemblyModeActive;
	ResetHoverSampleCache();
	ApplyAssemblyModeToFocusedSurfaceGrid();
	if (!ModeState.bAssemblyModeActive)
	{
		EndStructurePlacementDrag();
		ClearAreaSelection();
		ClearAreaDeletion();
		CancelAreaCopyPlacement();
		ClearPendingConveyorPathStart();
		PlacementQueue.Reset();
		ModeState.ResetStructurePlacementRotation();
		FSRAssemblyPreviewResetOptions PreviewResetOptions;
		PreviewResetOptions.bDestroyStructurePlacementDragPreviewActors = true;
		PreviewResetOptions.bDestroyConveyorDeletionGhostActor = true;
		FSRAssemblyPreviewReset::Apply(StructurePreview, ConveyorPreview, HoveredSurfaceGrid, PreviewResetOptions);
	}
}

void USRAssemblyComponent::ToggleAssemblyMode()
{
	SetAssemblyModeActive(!ModeState.bAssemblyModeActive);
}

bool USRAssemblyComponent::RotateStructurePlacement(int32 StepDelta)
{
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!ModeState.bAssemblyModeActive
		|| !IsValid(PlayerController)
		|| PlayerController->IsPointerOverBlockingUI()
		|| !IsValid(SelectedStructureDataAsset))
	{
		return false;
	}

	const FSRStructureData StructureData = SelectedStructureDataAsset->BuildData();
	if (StructureData.BuildKind != ESRStructureBuildKind::Structure)
	{
		return false;
	}

	if (!ModeState.RotateStructurePlacement(StepDelta))
	{
		return false;
	}

	StructurePreview.bHasStructureGhostCellId = false;
	UpdateStructureGhostPreview();
	return true;
}

int32 USRAssemblyComponent::GetStructurePlacementRotationSteps() const
{
	return ModeState.GetStructurePlacementRotationSteps();
}

float USRAssemblyComponent::GetStructurePlacementAdditionalYawDegrees() const
{
	return ModeState.GetStructurePlacementAdditionalYawDegrees();
}

void USRAssemblyComponent::ConfigurePlacementPerformance(int32 NewMaxStructurePlacementsPerFrame, int32 NewMaxQueuedStructurePlacements)
{
	PlacementQueue.ConfigurePerformance(NewMaxStructurePlacementsPerFrame, NewMaxQueuedStructurePlacements);
}

void USRAssemblyComponent::CancelSelectedStructurePlacement()
{
	EndStructurePlacementDrag(false);
	ClearPendingConveyorPathStart();
	PlacementQueue.Reset();
	FSRAssemblyPreviewResetOptions PreviewResetOptions;
	PreviewResetOptions.bClearConveyorBulkDeletionPreview = false;
	PreviewResetOptions.bDestroyStructurePlacementDragPreviewActors = true;
	FSRAssemblyPreviewReset::Apply(StructurePreview, ConveyorPreview, HoveredSurfaceGrid, PreviewResetOptions);

	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->SetHoveredInteractionGridPatchVisible(false);
	}
}

bool USRAssemblyComponent::TryHandleAssemblyClick(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;

	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController || !ModeState.bAssemblyModeActive)
	{
		return false;
	}

	if (AreaCopy.IsPlacementActive())
	{
		return TryCommitAreaCopyPlacement(OutSelectedActor);
	}

	ClearAreaSelection();
	ClearAreaDeletion();

	StarRovers::Assembly::FSRAssemblySurfaceCursorTarget CursorTarget;
	if (StarRovers::Assembly::FSRAssemblySurfaceCursorQuery::TryResolveSurfaceCell(PlayerController, CursorTarget))
	{
		OutSelectedActor = CursorTarget.FocusedActor;
		AActor* FocusedActor = CursorTarget.FocusedActor;
		USRPlanetSurfaceGrid* FocusedSurfaceGrid = CursorTarget.SurfaceGrid;
		const FSRPlanetSurfaceGridCell& HoveredCell = CursorTarget.Cell;
		if (TryPublishSelectedStructureInfo(FocusedActor, FocusedSurfaceGrid, HoveredCell))
		{
			ClearPendingConveyorPathStart();
			StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
			FocusedSurfaceGrid->SetSelectedCell(HoveredCell.CellId);
			PublishHoveredCellInfo(FocusedSurfaceGrid, HoveredCell);
			return true;
		}

		if (USRStructureDataAsset* SelectedStructureDataAsset = PlayerController->GetSelectedStructureDataAsset())
		{
			ClearSelectedStructureInfo();
			const FSRStructureData StructureData = SelectedStructureDataAsset->BuildData();
			if (StructureData.BuildKind == ESRStructureBuildKind::Conveyor)
			{
				ClearPendingConveyorPathStart();
				FocusedSurfaceGrid->SetSelectedCell(HoveredCell.CellId);
				PublishHoveredCellInfo(FocusedSurfaceGrid, HoveredCell);
				return true;
			}

			ClearPendingConveyorPathStart();
			FocusedSurfaceGrid->ClearSelectedCell();
			TryPlaceSelectedStructure(FocusedSurfaceGrid, HoveredCell);
			return true;
		}

		ClearPendingConveyorPathStart();
		ClearSelectedStructureInfo();
		FocusedSurfaceGrid->ClearSelectedCell();
		return true;
	}

	return false;
}

bool USRAssemblyComponent::TryHandleAssemblyDelete(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;

	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController || !ModeState.bAssemblyModeActive)
	{
		return false;
	}

	if (AreaCopy.IsPlacementActive())
	{
		CancelAreaCopyPlacement();
		return true;
	}

	ClearAreaSelection();
	ClearAreaDeletion();

	StarRovers::Assembly::FSRAssemblySurfaceCursorTarget CursorTarget;
	if (!StarRovers::Assembly::FSRAssemblySurfaceCursorQuery::TryResolveSurfaceCell(PlayerController, CursorTarget))
	{
		return false;
	}

	OutSelectedActor = CursorTarget.FocusedActor;
	AActor* FocusedActor = CursorTarget.FocusedActor;
	USRPlanetSurfaceGrid* FocusedSurfaceGrid = CursorTarget.SurfaceGrid;
	const FSRPlanetSurfaceGridCell& HoveredCell = CursorTarget.Cell;
	TArray<int32> CandidateConveyorLayers;
	StarRovers::Assembly::FSRAssemblySingleCellDeletion::BuildCandidateConveyorLayers(
		PlayerController->GetSelectedStructureDataAsset(),
		CandidateConveyorLayers);
	const bool bBulkDeleteConveyors = PlayerController->IsConveyorBulkDeleteModifierActive();
	const bool bDeleted = bBulkDeleteConveyors
		? StarRovers::Assembly::FSRAssemblySingleCellDeletion::TryDeleteConnectedConveyorsAtCell(
			FocusedActor,
			FocusedSurfaceGrid,
			HoveredCell.CellId,
			CandidateConveyorLayers)
		: StarRovers::Assembly::FSRAssemblySingleCellDeletion::TryDeleteStructureAtCell(
			FocusedActor,
			FocusedSurfaceGrid,
			HoveredCell.CellId,
			CandidateConveyorLayers);
	if (!bDeleted)
	{
		return true;
	}

	ConveyorPreview.ClearBulkDeletionPreview();
	ClearPendingConveyorPathStart();
	PlacementQueue.Reset();
	StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
	ClearSelectedStructureInfo();
	FocusedSurfaceGrid->SetHoveredCell(HoveredCell.CellId);
	FocusedSurfaceGrid->ClearSelectedCell();
	SurfaceState.ResetPublishedHoveredCellInfo();
	PublishHoveredCellInfo(FocusedSurfaceGrid, HoveredCell);
	return true;
}

bool USRAssemblyComponent::ShouldHandleStructurePlacementDrag() const
{
	const ASRPlayerController* PlayerController = GetOwnerController();
	const USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!ModeState.bAssemblyModeActive || AreaCopy.IsPlacementActive() || !IsValid(SelectedStructureDataAsset))
	{
		return false;
	}

	return true;
}

bool USRAssemblyComponent::BeginStructurePlacementDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (AreaCopy.IsPlacementActive())
	{
		return false;
	}

	EndStructurePlacementDrag(false);
	ClearAreaSelection();
	ClearAreaDeletion();

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	if (!TryResolveStructurePlacementDragTarget(FocusedActor, SurfaceGrid, TargetCell))
	{
		return false;
	}

	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (IsValid(SelectedStructureDataAsset) && SelectedStructureDataAsset->BuildData().BuildKind == ESRStructureBuildKind::Conveyor)
	{
		PlacementDrag.BeginConveyorPlacementDrag(SurfaceGrid, TargetCell.CellId);
		SurfaceGrid->SetSelectedCell(TargetCell.CellId);
		PublishHoveredCellInfo(SurfaceGrid, TargetCell);
		StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
		OutSelectedActor = FocusedActor;
		return UpdateConveyorGhostPreview(SurfaceGrid, TargetCell, SelectedStructureDataAsset);
	}

	PlacementDrag.BeginStructurePlacementDrag(SurfaceGrid, TargetCell.CellId, GetStructurePlacementRotationSteps());
	OutSelectedActor = FocusedActor;
	ConveyorPreview.DestroyGhostActor(HoveredSurfaceGrid);
	StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
	StructurePreview.ClearGhostPortPreview();
	return UpdateStructurePlacementDragPreview(SurfaceGrid, TargetCell);
}

bool USRAssemblyComponent::ContinueStructurePlacementDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!PlacementDrag.bIsStructurePlacementDragActive)
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	if (!TryResolveStructurePlacementDragTarget(FocusedActor, SurfaceGrid, TargetCell))
	{
		return false;
	}

	OutSelectedActor = FocusedActor;
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (PlacementDrag.bIsConveyorPlacementDragActive)
	{
		return IsValid(SelectedStructureDataAsset)
			&& SelectedStructureDataAsset->BuildData().BuildKind == ESRStructureBuildKind::Conveyor
			&& UpdateConveyorGhostPreview(SurfaceGrid, TargetCell, SelectedStructureDataAsset);
	}

	if (SurfaceGrid != PlacementDrag.StructurePlacementDragSurfaceGrid)
	{
		return false;
	}

	return UpdateStructurePlacementDragPreview(SurfaceGrid, TargetCell);
}

void USRAssemblyComponent::EndStructurePlacementDrag(bool bCommitConveyorDrag)
{
	if (bCommitConveyorDrag && PlacementDrag.bIsConveyorPlacementDragActive)
	{
		CommitConveyorPlacementDrag();
	}
	else if (bCommitConveyorDrag && PlacementDrag.bIsStructurePlacementDragActive)
	{
		CommitStructurePlacementDrag();
	}

	PlacementDrag.ResetPlacementDrag();
	StructurePreview.DestroyPlacementDragPreviewActors(HoveredSurfaceGrid);
	ConveyorPreview.DestroyGhostActor(HoveredSurfaceGrid);
	ConveyorPreview.ClearInvalidPlacementPreview();
}
