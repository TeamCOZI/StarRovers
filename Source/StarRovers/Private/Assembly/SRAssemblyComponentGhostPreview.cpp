#include "Assembly/SRAssemblyComponent.h"

#include "Assembly/SRAssemblyStructureDragPreviewUpdater.h"
#include "Assembly/SRAssemblyStructureGhostPreviewUpdater.h"
#include "Camera/SRPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

void USRAssemblyComponent::UpdateStructureGhostPreview()
{
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	const int32 RotationSteps = GetStructurePlacementRotationSteps();
	const StarRovers::Assembly::ESRAssemblyStructureGhostPreviewUpdateResult UpdateResult =
		StarRovers::Assembly::FSRAssemblyStructureGhostPreviewUpdater::Update(
			GetWorld(),
			PlayerController,
			ModeState.bAssemblyModeActive,
			HoveredSurfaceGrid,
			SelectedStructureDataAsset,
			RotationSteps,
			GetStructurePlacementAdditionalYawDegrees(),
			StructurePreview);
	if (UpdateResult == StarRovers::Assembly::ESRAssemblyStructureGhostPreviewUpdateResult::DestroyPreview)
	{
		StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
	}
}

bool USRAssemblyComponent::UpdateStructurePlacementDragPreview(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCell& TargetCell)
{
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* StructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!IsValid(SurfaceGrid)
		|| !IsValid(StructureDataAsset)
		|| !PlacementDrag.bIsStructurePlacementDragActive
		|| !IsValid(PlacementDrag.StructurePlacementDragSurfaceGrid)
		|| SurfaceGrid != PlacementDrag.StructurePlacementDragSurfaceGrid)
	{
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	if (StructureData.BuildKind != ESRStructureBuildKind::Structure)
	{
		return false;
	}

	if (PlacementDrag.IsLastStructurePlacementDragCell(SurfaceGrid, TargetCell.CellId)
		&& !PlacementDrag.StructurePlacementDragCellIds.IsEmpty())
	{
		return true;
	}

	TArray<FSRPlanetSurfaceGridCellId> CandidateCellIds;
	if (!AreaSelection.BuildCellIds(SurfaceGrid, PlacementDrag.StructurePlacementDragStartCellId, TargetCell.CellId, CandidateCellIds))
	{
		return false;
	}

	const int32 PlacementRotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(PlacementDrag.StructurePlacementDragRotationSteps);
	if (!StarRovers::Assembly::FSRAssemblyStructureDragPreviewUpdater::Update(
		GetWorld(),
		PlayerController,
		SurfaceGrid,
		StructureDataAsset,
		StructureData,
		CandidateCellIds,
		PlacementRotationSteps,
		GetStructurePlacementAdditionalYawDegrees(),
		PlacementDrag,
		StructurePreview))
	{
		return false;
	}
	SurfaceGrid->SetHoveredCell(TargetCell.CellId);
	PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	PlacementDrag.SetLastStructurePlacementDragCell(SurfaceGrid, TargetCell.CellId);
	StructurePreview.ClearGhostPortPreview();
	return true;
}

