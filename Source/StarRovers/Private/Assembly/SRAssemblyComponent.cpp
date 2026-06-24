#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRPlayerController.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

USRAssemblyComponent::USRAssemblyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bAssemblyModeActive = false;
	MaxStructurePlacementsPerFrame = 4;
	MaxQueuedStructurePlacements = 256;
	ActiveAssemblySurfaceGrid = nullptr;
	LastHoveredSampleSurfaceGrid = nullptr;
	LastPublishedHoveredSurfaceGrid = nullptr;
	StructureGhostActor = nullptr;
	StructureGhostDataAsset = nullptr;
	StructureGhostPortPreviewSurfaceGrid = nullptr;
	ConveyorPortPreviewSurfaceGrid = nullptr;
	ConveyorGhostActor = nullptr;
	ConveyorDeletionGhostActor = nullptr;
	ConveyorGhostDataAsset = nullptr;
	ConveyorDeletionGhostDataAsset = nullptr;
	ConveyorGhostSurfaceGrid = nullptr;
	ConveyorDeletionGhostSurfaceGrid = nullptr;
	ConveyorBulkDeletionPreviewSurfaceGrid = nullptr;
	LastLoggedInvalidGhostDataAsset = nullptr;
	LastHoveredSampleMousePosition = FVector2D::ZeroVector;
	bHasLastHoveredSampleMousePosition = false;
	bHasLastPublishedHoveredCellInfo = false;
	LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
	StructureGhostCellId = FSRPlanetSurfaceGridCellId();
	bHasStructureGhostCellId = false;
	bHasStructureGhostPortPreview = false;
	bHasConveyorPortPreview = false;
	bIsStructurePlacementDragActive = false;
	bIsConveyorPlacementDragActive = false;
	StructurePlacementRotationSteps = 0;
	LastStructurePlacementDragSurfaceGrid = nullptr;
	LastStructurePlacementDragCellId = FSRPlanetSurfaceGridCellId();
	bHasLastStructurePlacementDragCellId = false;
	PendingConveyorStartSurfaceGrid = nullptr;
	PendingConveyorStartCellId = FSRPlanetSurfaceGridCellId();
	bHasPendingConveyorStartCell = false;
	ConveyorDragStartSurfaceGrid = nullptr;
	ConveyorDragWaypointCellIds.Reset();
	ConveyorDragStartCellId = FSRPlanetSurfaceGridCellId();
	ConveyorGhostTargetCellId = FSRPlanetSurfaceGridCellId();
	ConveyorDeletionGhostTargetCellId = FSRPlanetSurfaceGridCellId();
	bHasConveyorDragStartCell = false;
	bHasConveyorGhostTargetCell = false;
	bHasConveyorDeletionGhostTargetCell = false;
	bHasConveyorBulkDeletionPreview = false;
	ConveyorDeletionGhostLayer = 0;
}

void USRAssemblyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateSurfaceHover();
	ProcessQueuedStructurePlacements();
	const ASRPlayerController* PlayerController = GetOwnerController();
	if (PlayerController && PlayerController->IsConveyorBulkDeleteModifierActive())
	{
		if (!UpdateConveyorBulkDeletionPreview())
		{
			ClearConveyorBulkDeletionPreview();
		}
		ClearConveyorPlacementPortPreview();
		DestroyStructureGhostPreview();
		DestroyConveyorGhostPreview();
		return;
	}

	ClearConveyorBulkDeletionPreview();
	UpdateConveyorPlacementPortPreview();
	UpdateStructureGhostPreview();
}

void USRAssemblyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearConveyorPlacementPortPreview();
	ClearConveyorBulkDeletionPreview();
	DestroyStructureGhostPreview();
	DestroyConveyorGhostPreview();
	DestroyConveyorDeletionGhostPreview();

	Super::EndPlay(EndPlayReason);
}

bool USRAssemblyComponent::IsAssemblyModeActive() const
{
	return bAssemblyModeActive;
}

void USRAssemblyComponent::SetAssemblyModeActive(bool bNewAssemblyModeActive)
{
	if (bNewAssemblyModeActive)
	{
		AActor* FocusedActor = nullptr;
		USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
		if (!TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid))
		{
			bNewAssemblyModeActive = false;
		}
	}

	if (bAssemblyModeActive == bNewAssemblyModeActive)
	{
		ApplyAssemblyModeToFocusedSurfaceGrid();
		return;
	}

	bAssemblyModeActive = bNewAssemblyModeActive;
	ResetHoverSampleCache();
	ApplyAssemblyModeToFocusedSurfaceGrid();
	if (!bAssemblyModeActive)
	{
		EndStructurePlacementDrag();
		ClearPendingConveyorPathStart();
		PendingStructurePlacementQueue.Reset();
		StructurePlacementRotationSteps = 0;
		ClearConveyorPlacementPortPreview();
		ClearConveyorBulkDeletionPreview();
		DestroyStructureGhostPreview();
		DestroyConveyorGhostPreview();
		DestroyConveyorDeletionGhostPreview();
	}
}

void USRAssemblyComponent::ToggleAssemblyMode()
{
	SetAssemblyModeActive(!bAssemblyModeActive);
}

bool USRAssemblyComponent::RotateStructurePlacement(int32 StepDelta)
{
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!bAssemblyModeActive
		|| !IsValid(PlayerController)
		|| PlayerController->IsPointerOverBlockingUi()
		|| !IsValid(SelectedStructureDataAsset))
	{
		return false;
	}

	const FSRStructureData StructureData = SelectedStructureDataAsset->BuildData();
	if (StructureData.BuildKind != ESRStructureBuildKind::Structure)
	{
		return false;
	}

	const int32 PreviousRotationSteps = StructurePlacementRotationSteps;
	StructurePlacementRotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(StructurePlacementRotationSteps + StepDelta);
	if (StructurePlacementRotationSteps == PreviousRotationSteps)
	{
		return false;
	}

	bHasStructureGhostCellId = false;
	UpdateStructureGhostPreview();
	return true;
}

int32 USRAssemblyComponent::GetStructurePlacementRotationSteps() const
{
	return StarRovers::Structure::NormalizePlacementRotationSteps(StructurePlacementRotationSteps);
}

float USRAssemblyComponent::GetStructurePlacementAdditionalYawDegrees() const
{
	return StarRovers::Structure::PlacementRotationStepsToYawDegrees(StructurePlacementRotationSteps);
}

void USRAssemblyComponent::ConfigurePlacementPerformance(int32 NewMaxStructurePlacementsPerFrame, int32 NewMaxQueuedStructurePlacements)
{
	MaxStructurePlacementsPerFrame = FMath::Max(1, NewMaxStructurePlacementsPerFrame);
	MaxQueuedStructurePlacements = FMath::Max(1, NewMaxQueuedStructurePlacements);
}

bool USRAssemblyComponent::TryHandleAssemblyClick(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;

	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController || !bAssemblyModeActive)
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell HoveredCell;
	FVector HoverHitLocation = FVector::ZeroVector;

	if (TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid)
		&& TryProjectCursorToSurfaceCell(FocusedSurfaceGrid, HoveredCell, HoverHitLocation))
	{
		OutSelectedActor = FocusedActor;
		if (TryPublishSelectedStructureInfo(FocusedActor, FocusedSurfaceGrid, HoveredCell))
		{
			ClearPendingConveyorPathStart();
			DestroyStructureGhostPreview();
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
		FocusedSurfaceGrid->SetSelectedCell(HoveredCell.CellId);
		return true;
	}

	return false;
}

bool USRAssemblyComponent::TryHandleAssemblyDelete(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;

	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController || !bAssemblyModeActive)
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell HoveredCell;
	FVector HoverHitLocation = FVector::ZeroVector;
	if (!TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid)
		|| !TryProjectCursorToSurfaceCell(FocusedSurfaceGrid, HoveredCell, HoverHitLocation))
	{
		return false;
	}

	OutSelectedActor = FocusedActor;
	const bool bBulkDeleteConveyors = PlayerController->IsConveyorBulkDeleteModifierActive();
	const bool bDeleted = bBulkDeleteConveyors
		? TryDeleteConnectedConveyorsAtCell(FocusedActor, FocusedSurfaceGrid, HoveredCell.CellId)
		: TryDeleteStructureAtCell(FocusedActor, FocusedSurfaceGrid, HoveredCell.CellId);
	if (!bDeleted)
	{
		return true;
	}

	ClearConveyorBulkDeletionPreview();
	ClearPendingConveyorPathStart();
	PendingStructurePlacementQueue.Reset();
	DestroyStructureGhostPreview();
	ClearSelectedStructureInfo();
	FocusedSurfaceGrid->SetHoveredCell(HoveredCell.CellId);
	FocusedSurfaceGrid->ClearSelectedCell();
	bHasLastPublishedHoveredCellInfo = false;
	LastPublishedHoveredSurfaceGrid = nullptr;
	LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
	PublishHoveredCellInfo(FocusedSurfaceGrid, HoveredCell);
	return true;
}

bool USRAssemblyComponent::ShouldHandleStructurePlacementDrag() const
{
	const ASRPlayerController* PlayerController = GetOwnerController();
	const USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!bAssemblyModeActive || !IsValid(SelectedStructureDataAsset))
	{
		return false;
	}

	return true;
}

bool USRAssemblyComponent::BeginStructurePlacementDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	EndStructurePlacementDrag(false);

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
		bIsStructurePlacementDragActive = true;
		bIsConveyorPlacementDragActive = true;
		ConveyorDragStartSurfaceGrid = SurfaceGrid;
		ConveyorDragWaypointCellIds.Reset();
		ConveyorDragStartCellId = TargetCell.CellId;
		bHasConveyorDragStartCell = true;
		LastStructurePlacementDragSurfaceGrid = SurfaceGrid;
		LastStructurePlacementDragCellId = TargetCell.CellId;
		bHasLastStructurePlacementDragCellId = true;
		SurfaceGrid->SetSelectedCell(TargetCell.CellId);
		PublishHoveredCellInfo(SurfaceGrid, TargetCell);
		DestroyStructureGhostPreview();
		OutSelectedActor = FocusedActor;
		return UpdateConveyorGhostPreview(SurfaceGrid, TargetCell, SelectedStructureDataAsset);
	}

	bIsStructurePlacementDragActive = true;
	OutSelectedActor = FocusedActor;
	DestroyConveyorGhostPreview();
	return TryPlaceStructureDragPath(SurfaceGrid, TargetCell);
}

bool USRAssemblyComponent::ContinueStructurePlacementDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!bIsStructurePlacementDragActive)
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
	if (bIsConveyorPlacementDragActive)
	{
		return IsValid(SelectedStructureDataAsset)
			&& SelectedStructureDataAsset->BuildData().BuildKind == ESRStructureBuildKind::Conveyor
			&& UpdateConveyorGhostPreview(SurfaceGrid, TargetCell, SelectedStructureDataAsset);
	}

	return TryPlaceStructureDragPath(SurfaceGrid, TargetCell);
}

void USRAssemblyComponent::EndStructurePlacementDrag(bool bCommitConveyorDrag)
{
	if (bCommitConveyorDrag && bIsConveyorPlacementDragActive)
	{
		CommitConveyorPlacementDrag();
	}

	bIsStructurePlacementDragActive = false;
	bIsConveyorPlacementDragActive = false;
	LastStructurePlacementDragSurfaceGrid = nullptr;
	LastStructurePlacementDragCellId = FSRPlanetSurfaceGridCellId();
	bHasLastStructurePlacementDragCellId = false;
	ConveyorDragStartSurfaceGrid = nullptr;
	ConveyorDragWaypointCellIds.Reset();
	ConveyorDragStartCellId = FSRPlanetSurfaceGridCellId();
	bHasConveyorDragStartCell = false;
	DestroyConveyorGhostPreview();
}
