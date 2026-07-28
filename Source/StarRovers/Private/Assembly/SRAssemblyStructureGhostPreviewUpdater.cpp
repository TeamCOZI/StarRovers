#include "Assembly/SRAssemblyStructureGhostPreviewUpdater.h"

#include "Assembly/SRAssemblyConstructionReplacement.h"
#include "Assembly/SRAssemblyPreviewState.h"
#include "Assembly/SRAssemblyPreviewMaterial.h"
#include "Assembly/SRAssemblyStructurePlacementPreview.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
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
		FSRAssemblyConveyorPreviewState& ConveyorPreview,
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
		if (!HoveredSurfaceGrid->GetCellInfoById(HoveredCell.CellId, HoveredCellInfo))
		{
			return ESRAssemblyStructureGhostPreviewUpdateResult::DestroyPreview;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (StructureData.BuildKind == ESRStructureBuildKind::Conveyor)
		{
			return ESRAssemblyStructureGhostPreviewUpdateResult::DestroyPreview;
		}

		FSRStructurePlacementEvaluation PlacementEvaluation =
			FSRAssemblyStructurePlacementPreviewEvaluator::Evaluate(
				HoveredSurfaceGrid,
				StructureDataAsset,
				RotationSteps);

		AActor* SurfaceOwner = HoveredSurfaceGrid->GetOwner();
		USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
			: nullptr;
		USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
			: nullptr;
		if (!PlacementEvaluation.Preview.bCanPlace)
		{
			if (IsValid(StructureInstanceManager))
			{
				StructureInstanceManager->SetConstructionReplacementPreviewedStructures(TSet<FName>());
			}
			HoveredSurfaceGrid->ClearConstructionReplacementPreviewCells();
			ConveyorPreview.ClearBulkDeletionPreview();
			StructurePreview.SetInvalidPlacementPreview(
				HoveredSurfaceGrid,
				PlacementEvaluation.FootprintCellIds);
			return ESRAssemblyStructureGhostPreviewUpdateResult::BlockedPreview;
		}
		StructurePreview.ClearInvalidPlacementPreview();

		ConstructionReplacement::FSRConstructionReplacementTargets ReplacementTargets;
		ReplacementTargets.StructureOccupantIds = MoveTemp(PlacementEvaluation.ReplacementStructureIds);
		ReplacementTargets.ConveyorCellIds = MoveTemp(PlacementEvaluation.ReplacementConveyorCellIds);
		ReplacementTargets.ConveyorBeltPaths = MoveTemp(PlacementEvaluation.ReplacementConveyorBeltPaths);
		const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds = PlacementEvaluation.FootprintCellIds;
		if (IsValid(StructureInstanceManager))
		{
			StructureInstanceManager->SetConstructionReplacementPreviewedStructures(ReplacementTargets.StructureOccupantIds);
		}
		ConstructionReplacement::ApplyConstructionReplacementPreview(
			HoveredSurfaceGrid,
			StructureInstanceManager,
			ReplacementTargets);
		ConstructionReplacement::ApplyConveyorReplacementPreview(
			HoveredSurfaceGrid,
			ConveyorNetwork,
			ConveyorPreview,
			ReplacementTargets.ConveyorCellIds,
			ReplacementTargets.ConveyorBeltPaths,
			HoveredCell.CellId);

		FSRPlanetSurfaceGridCellInfo PreviewCellInfo = HoveredCellInfo;
		if (PreviewCellInfo.bOccupied
			&& ((!PreviewCellInfo.OccupantId.IsNone()
				&& ReplacementTargets.StructureOccupantIds.Contains(PreviewCellInfo.OccupantId))
				|| ReplacementTargets.ConveyorCellIds.Contains(HoveredCell.CellId)))
		{
			PreviewCellInfo.bOccupied = false;
			PreviewCellInfo.bCanConstruct = true;
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
			StructurePreview.SetInvalidPlacementPreview(HoveredSurfaceGrid, FootprintCellIds);
			return ESRAssemblyStructureGhostPreviewUpdateResult::BlockedPreview;
		}

		UMaterialInterface* PreviewMaterial = ReplacementTargets.HasAny()
			? PreviewMaterials::ResolveReplaceableMaterial(StructureData)
			: PreviewMaterials::ResolveGhostMaterial(StructureData);
		HoveredSurfaceGrid->SetPlacementPreviewCells(FootprintCellIds);
		if (!StructurePreview.UpdateGhostActor(
			World,
			PreviewOwner,
			HoveredSurfaceGrid,
			StructureDataAsset,
			StructureData,
			GhostTransform,
			PreviewCellInfo,
			PreviewMaterial))
		{
			StructurePreview.SetInvalidPlacementPreview(HoveredSurfaceGrid, FootprintCellIds);
			return ESRAssemblyStructureGhostPreviewUpdateResult::BlockedPreview;
		}

		StructurePreview.UpdateGhostPortPreview(HoveredSurfaceGrid, StructureData, FootprintCellIds, RotationSteps);
		StructurePreview.StructureGhostCellId = HoveredCell.CellId;
		StructurePreview.bHasStructureGhostCellId = true;
		return ESRAssemblyStructureGhostPreviewUpdateResult::Updated;
	}
}
