#include "Assembly/SRAssemblyStructureGhostPreviewUpdater.h"

#include "Assembly/SRAssemblyPreviewState.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Structure/SRStructurePlacementLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Assembly
{
	ESRAssemblyStructureGhostPreviewUpdateResult FSRAssemblyStructureGhostPreviewUpdater::Update(
		UWorld* World,
		AActor* PreviewOwner,
		bool bAssemblyModeActive,
		USRPlanetSurfaceGrid* HoveredSurfaceGrid,
		USRStructureDataAsset* StructureDataAsset,
		int32 RotationSteps,
		float AdditionalYawDegrees,
		FSRAssemblyStructurePreviewState& StructurePreview)
	{
		if (!bAssemblyModeActive
			|| !IsValid(PreviewOwner)
			|| !IsValid(StructureDataAsset)
			|| !IsValid(HoveredSurfaceGrid))
		{
			return ESRAssemblyStructureGhostPreviewUpdateResult::DestroyPreview;
		}

		FSRPlanetSurfaceGridCell HoveredCell;
		if (!HoveredSurfaceGrid->GetHoveredCell(HoveredCell))
		{
			return ESRAssemblyStructureGhostPreviewUpdateResult::DestroyPreview;
		}

		FSRPlanetSurfaceGridCellInfo HoveredCellInfo;
		if (!HoveredSurfaceGrid->GetCellInfoById(HoveredCell.CellId, HoveredCellInfo)
			|| !HoveredCellInfo.bCanConstruct)
		{
			return ESRAssemblyStructureGhostPreviewUpdateResult::DestroyPreview;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (StructureData.BuildKind == ESRStructureBuildKind::Conveyor)
		{
			return ESRAssemblyStructureGhostPreviewUpdateResult::DestroyPreview;
		}

		const int32 FootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, RotationSteps);
		const int32 FootprintCellsY = StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, RotationSteps);

		TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
		if (!HoveredSurfaceGrid->GetFootprintCellIds(HoveredCell.CellId, FootprintCellsX, FootprintCellsY, FootprintCellIds))
		{
			return ESRAssemblyStructureGhostPreviewUpdateResult::DestroyPreview;
		}

		AActor* SurfaceOwner = HoveredSurfaceGrid->GetOwner();
		USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
			: nullptr;
		TSet<FName> ReplaceableOccupantIds;
		const bool bCanBuildOverCells = IsValid(StructureInstanceManager)
			? StructureInstanceManager->CanBuildOverCellsForConstruction(HoveredSurfaceGrid, FootprintCellIds, ReplaceableOccupantIds)
			: HoveredSurfaceGrid->CanOccupyCells(FootprintCellIds);
		if (!bCanBuildOverCells)
		{
			if (IsValid(StructureInstanceManager))
			{
				StructureInstanceManager->SetConstructionReplacementPreviewedStructures(ReplaceableOccupantIds);
			}
			return ESRAssemblyStructureGhostPreviewUpdateResult::DestroyPreview;
		}
		if (IsValid(StructureInstanceManager))
		{
			StructureInstanceManager->SetConstructionReplacementPreviewedStructures(ReplaceableOccupantIds);
		}

		FSRPlanetSurfaceGridCellInfo PreviewCellInfo = HoveredCellInfo;
		if (PreviewCellInfo.bOccupied
			&& !PreviewCellInfo.OccupantId.IsNone()
			&& ReplaceableOccupantIds.Contains(PreviewCellInfo.OccupantId))
		{
			PreviewCellInfo.bOccupied = false;
			PreviewCellInfo.OccupantId = NAME_None;
		}

		FTransform GhostTransform;
		if (!USRStructurePlacementLibrary::BuildStructurePlacementTransform(
			HoveredSurfaceGrid,
			HoveredCell.CellId,
			StructureDataAsset,
			GhostTransform,
			AdditionalYawDegrees))
		{
			return ESRAssemblyStructureGhostPreviewUpdateResult::DestroyPreview;
		}

		if (!StructurePreview.UpdateGhostActor(World, PreviewOwner, HoveredSurfaceGrid, StructureDataAsset, StructureData, GhostTransform, PreviewCellInfo))
		{
			return ESRAssemblyStructureGhostPreviewUpdateResult::NoChange;
		}

		StructurePreview.UpdateGhostPortPreview(HoveredSurfaceGrid, StructureData, FootprintCellIds, RotationSteps);
		StructurePreview.StructureGhostCellId = HoveredCell.CellId;
		StructurePreview.bHasStructureGhostCellId = true;
		return ESRAssemblyStructureGhostPreviewUpdateResult::Updated;
	}
}
