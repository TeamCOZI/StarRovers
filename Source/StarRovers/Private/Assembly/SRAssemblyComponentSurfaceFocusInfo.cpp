#include "Assembly/SRAssemblyComponent.h"

#include "Assembly/SRAssemblyConveyorPortPreviewUpdater.h"
#include "Assembly/SRAssemblySurfaceFocusInfoBuilder.h"
#include "Assembly/SRAssemblySurfaceCursorQuery.h"
#include "Camera/SRPlayerController.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

void USRAssemblyComponent::PublishHoveredCellInfo(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& HoveredCell)
{
	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController || !IsValid(SurfaceGrid))
	{
		return;
	}

	if (SurfaceState.bHasLastPublishedHoveredCellInfo
		&& SurfaceState.LastPublishedHoveredSurfaceGrid == SurfaceGrid
		&& SurfaceState.LastPublishedHoveredCellId == HoveredCell.CellId)
	{
		return;
	}

	FSRPlanetSurfaceGridCellInfo HoveredCellInfo;
	if (!SurfaceGrid->GetCellInfoById(HoveredCell.CellId, HoveredCellInfo))
	{
		ClearPublishedHoveredCellInfo();
		return;
	}

	SurfaceState.bHasLastPublishedHoveredCellInfo = true;
	SurfaceState.LastPublishedHoveredSurfaceGrid = SurfaceGrid;
	SurfaceState.LastPublishedHoveredCellId = HoveredCell.CellId;
	PlayerController->SetHoveredSurfaceCellInfo(true, HoveredCellInfo);
}

void USRAssemblyComponent::ClearPublishedHoveredCellInfo()
{
	if (!SurfaceState.bHasLastPublishedHoveredCellInfo)
	{
		return;
	}

	SurfaceState.ResetPublishedHoveredCellInfo();
	if (ASRPlayerController* PlayerController = GetOwnerController())
	{
		PlayerController->SetHoveredSurfaceCellInfo(false, FSRPlanetSurfaceGridCellInfo());
	}
}

void USRAssemblyComponent::UpdateConveyorPlacementPortPreview()
{
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!ModeState.bAssemblyModeActive
		|| !IsValid(PlayerController)
		|| PlayerController->IsPointerOverBlockingUI()
		|| !StarRovers::Assembly::FSRAssemblyConveyorPortPreviewUpdater::Update(
			HoveredSurfaceGrid,
			SelectedStructureDataAsset,
			ConveyorPreview))
	{
		ConveyorPreview.ClearPortPreview();
	}
}

bool USRAssemblyComponent::TryPublishSelectedStructureInfo(AActor* FocusedActor, USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& ClickedCell)
{
	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController || !IsValid(FocusedActor) || !IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRFocusedSurfaceStructureInfo StructureInfo;
	if (!StarRovers::Assembly::FSRAssemblySurfaceFocusInfoBuilder::TryBuildSelectedStructureInfo(
		FocusedActor,
		SurfaceGrid,
		ClickedCell,
		StructureInfo))
	{
		return false;
	}

	TArray<FSRPlanetSurfaceGridCellId> InputConnectionCellIds;
	TArray<FSRPlanetSurfaceGridCellId> OutputConnectionCellIds;
	StarRovers::Assembly::FSRAssemblySurfaceFocusInfoBuilder::GatherFacilityPortPreviewCells(
		StructureInfo.FacilityPorts,
		InputConnectionCellIds,
		OutputConnectionCellIds);
	SurfaceGrid->SetSelectedFootprintCells(StructureInfo.FootprintCellIds);
	SurfaceGrid->SetOccupiedPreviewCells(StructureInfo.FootprintCellIds);
	SurfaceGrid->SetFacilityPortPreviewCells(InputConnectionCellIds, OutputConnectionCellIds);

	PlayerController->SetSelectedActorSurfaceStructureInfo(FocusedActor, StructureInfo);
	return true;
}

void USRAssemblyComponent::ClearSelectedStructureInfo()
{
	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
	if (StarRovers::Assembly::FSRAssemblySurfaceCursorQuery::TryGetFocusedSurfaceGrid(GetOwnerController(), FocusedActor, FocusedSurfaceGrid))
	{
		FocusedSurfaceGrid->ClearSelectedFootprintCells();
		FocusedSurfaceGrid->ClearOccupiedPreviewCells();
		FocusedSurfaceGrid->ClearFacilityPortPreviewCells();
	}
	else if (IsValid(SurfaceState.ActiveAssemblySurfaceGrid))
	{
		SurfaceState.ActiveAssemblySurfaceGrid->ClearSelectedFootprintCells();
		SurfaceState.ActiveAssemblySurfaceGrid->ClearOccupiedPreviewCells();
		SurfaceState.ActiveAssemblySurfaceGrid->ClearFacilityPortPreviewCells();
	}

	if (ASRPlayerController* PlayerController = GetOwnerController())
	{
		PlayerController->SetSelectedSurfaceStructureInfo(false, FSRFocusedSurfaceStructureInfo());
	}
}

void USRAssemblyComponent::ClearSelectedStructureFocus()
{
	ClearSelectedStructureInfo();
}
